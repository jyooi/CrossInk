#!/usr/bin/env python3
"""Catalog-semantics tests for the SD-card font build scripts.

Run with the font venv:

    .venv-fonts/bin/python lib/EpdFont/scripts/test_sd_fonts_catalog.py

These tests load the real sd-fonts.yaml through the same loader functions the
build scripts use, so they check meaning rather than file text. Nothing is
downloaded. Two tests build one small probe family from fonts in the repo to
check which face supplies a fallback glyph.
"""

from __future__ import annotations

import importlib.util
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

import yaml

SCRIPT_DIR = Path(__file__).resolve().parent
CATALOG = SCRIPT_DIR / "sd-fonts.yaml"
CJK_FAMILIES = ("LXGWWenKai", "LXGWWenKaiTC")

# Codepoints that patched_intervals adds to every SD family. The CJK extra
# fallbacks must not claim them, or ChareInk7 / NotoSymbols / NotoSans-Regular
# become unreachable for these glyphs.
NON_CJK_PATCHED_CODEPOINTS = (0x2669, 0x03BB, 0x2113, 0x0400)


def load_module(file_name: str, module_name: str):
    """Import a build script that has a hyphenated file name."""
    spec = importlib.util.spec_from_file_location(module_name, SCRIPT_DIR / file_name)
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


sd_fonts = load_module("build-sd-fonts.py", "build_sd_fonts")
dictionary_fonts = load_module("build-dictionary-fonts.py", "build_dictionary_fonts")
cjk_fonts = load_module("build-cjk-fonts.py", "build_cjk_fonts")


def read_catalog() -> list[dict]:
    with CATALOG.open() as handle:
        return yaml.safe_load(handle)["families"]


def covers(ranges, codepoint: int) -> bool:
    return any(start <= codepoint <= end for start, end in ranges or ())


class OptionalFamilyFilter(unittest.TestCase):
    """The optional key must keep CJK families out of every default build."""

    def test_cjk_families_are_optional(self):
        optional = {f["name"] for f in read_catalog() if f.get("optional")}
        self.assertEqual(optional, set(CJK_FAMILIES))

    def test_dictionary_wrapper_drops_optional_families(self):
        """Regression: the wrapper packaged families the builder never built."""
        _config, families = dictionary_fonts.load_dictionary_config(CATALOG)
        names = {family["name"] for family in families}
        self.assertTrue(names.isdisjoint(CJK_FAMILIES))
        expected = {f["name"] for f in read_catalog() if not f.get("optional")}
        self.assertEqual(names, expected)

    def test_default_build_rejects_an_all_optional_catalog(self):
        """A catalog of only optional families must not silently build nothing."""
        with tempfile.TemporaryDirectory() as work_dir:
            config_path = Path(work_dir) / "only-optional.yaml"
            config_path.write_text(
                yaml.safe_dump(
                    {
                        "families": [
                            {
                                "name": "OnlyOptional",
                                "intervals": "builtin",
                                "sizes": [8],
                                "optional": True,
                                "styles": {"regular": {"url": "https://example.invalid/x.ttf"}},
                            }
                        ]
                    }
                )
            )
            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT_DIR / "build-sd-fonts.py"),
                    "--config",
                    str(config_path),
                    "--output-dir",
                    str(Path(work_dir) / "out"),
                ],
                capture_output=True,
                text=True,
            )
        self.assertEqual(result.returncode, 1)
        self.assertIn("no default families", result.stderr)

    def test_cjk_wrapper_selects_both_cjk_families(self):
        _config, families = cjk_fonts.load_cjk_config(CATALOG)
        self.assertEqual([family["name"] for family in families], list(CJK_FAMILIES))


class ExtraFallbackRanges(unittest.TestCase):
    """CJK extra fallbacks must stay inside the CJK blocks they fill."""

    def test_every_cjk_extra_fallback_is_range_restricted(self):
        for family in read_catalog():
            if family["name"] not in CJK_FAMILIES:
                continue
            for style in family["styles"]:
                specs = sd_fonts.extra_fallback_specs(family, style)
                self.assertTrue(specs, f"{family['name']}/{style} has no extra fallbacks")
                for spec in specs:
                    ranges = sd_fonts.parse_extra_fallback_ranges(spec.get("ranges"))
                    with self.subTest(family=family["name"], style=style, spec=spec):
                        self.assertIsNotNone(ranges, "unrestricted extra fallback")
                        self.assertTrue(covers(ranges, 0x4E00), "CJK Unified not covered")
                        self.assertTrue(covers(ranges, 0x3400), "CJK Ext A not covered")
                        self.assertTrue(covers(ranges, 0x3105), "Bopomofo not covered")
                        for codepoint in NON_CJK_PATCHED_CODEPOINTS:
                            self.assertFalse(
                                covers(ranges, codepoint),
                                f"U+{codepoint:04X} must come from the built-in stack",
                            )

    def test_traditional_family_prefers_simplified_wenkai_then_noto(self):
        family = next(f for f in read_catalog() if f["name"] == "LXGWWenKaiTC")
        regular = sd_fonts.extra_fallback_specs(family, "regular")
        bold = sd_fonts.extra_fallback_specs(family, "bold")
        self.assertIn("LXGWWenKai-Regular.ttf", regular[0]["url"])
        self.assertIn("NotoSansCJKsc", regular[-1]["path"])
        self.assertIn("LXGWWenKai-Medium.ttf", bold[0]["url"])
        self.assertIn("NotoSansCJKsc", bold[-1]["path"])

    def test_per_style_override_replaces_the_generic_list(self):
        family = {
            "name": "Demo",
            "extra_fallbacks": [{"path": "generic.ttf"}],
            "extra_fallbacks_bold": [{"path": "bold.ttf"}],
        }
        self.assertEqual(sd_fonts.extra_fallback_specs(family, "bold"), [{"path": "bold.ttf"}])
        self.assertEqual(
            sd_fonts.extra_fallback_specs(family, "regular"), [{"path": "generic.ttf"}]
        )

    def test_bad_range_strings_are_rejected(self):
        for bad in (["0x4E00"], ["0x9FFF-0x4E00"], ["0x0-0x110000"]):
            with self.subTest(value=bad):
                with self.assertRaises(ValueError):
                    sd_fonts.parse_extra_fallback_ranges(bad)


class ConfigValidation(unittest.TestCase):
    def base_family(self, **extra) -> dict:
        family = {
            "name": "Demo",
            "intervals": "builtin",
            "sizes": [8],
            "styles": {"regular": {"path": "a.ttf"}, "bold": {"path": "b.ttf"}},
        }
        family.update(extra)
        return family

    def test_real_catalog_validates(self):
        self.assertEqual(sd_fonts.validate_config(read_catalog()), [])

    def test_unknown_extra_fallback_style_key_is_an_error(self):
        """Regression: a typo silently swapped the fallback stack."""
        errors = sd_fonts.validate_config(
            [self.base_family(extra_fallbacks_italic=[{"path": "x.ttf"}])]
        )
        self.assertTrue(any("extra_fallbacks_italic" in error for error in errors), errors)

    def test_declared_style_key_is_accepted(self):
        self.assertEqual(
            sd_fonts.validate_config([self.base_family(extra_fallbacks_bold=[{"path": "x.ttf"}])]),
            [],
        )

    def test_extra_fallback_needs_exactly_one_source(self):
        errors = sd_fonts.validate_config(
            [self.base_family(extra_fallbacks=[{"path": "x.ttf", "url": "https://e.invalid/x"}])]
        )
        self.assertTrue(any("exactly one" in error for error in errors), errors)


class ExtraFallbackRangesEndToEnd(unittest.TestCase):
    """Build a tiny family twice and check which face supplied one glyph.

    U+2669 is a patched interval that build-sd-fonts adds to every family.
    NotoSansSymbols owns it in the built-in fallback stack. Noto Sans CJK SC
    also has it, so an unrestricted CJK extra fallback steals it.
    """

    MUSIC_NOTE = 0x2669
    EPD_DIR = SCRIPT_DIR.parent
    NOTO_SYMBOLS = EPD_DIR / "builtinFonts/source/NotoSymbols/NotoSansSymbols-Regular.ttf"
    NOTO_CJK = EPD_DIR / "builtinFonts/source/NotoSansCJKsc/NotoSansCJKsc-Regular.otf"

    def build_probe(self, work_dir: Path, ranges: list[str] | None) -> Path:
        fallback = {"path": "builtinFonts/source/NotoSansCJKsc/NotoSansCJKsc-Regular.otf"}
        if ranges is not None:
            fallback["ranges"] = ranges
        config_path = work_dir / "probe.yaml"
        config_path.write_text(
            yaml.safe_dump(
                {
                    "families": [
                        {
                            "name": "RangeProbe",
                            "description": "probe",
                            "languages": "Latin",
                            "intervals": "builtin",
                            "sizes": [8],
                            "extra_fallbacks": [fallback],
                            "styles": {
                                "regular": {"path": "builtinFonts/source/NotoSans/NotoSans-Regular.ttf"}
                            },
                        }
                    ]
                }
            )
        )
        out_dir = work_dir / "out"
        result = subprocess.run(
            [
                sys.executable,
                str(SCRIPT_DIR / "build-sd-fonts.py"),
                "--config",
                str(config_path),
                "--output-dir",
                str(out_dir),
                "--no-package",
            ],
            capture_output=True,
            text=True,
        )
        self.assertEqual(result.returncode, 0, result.stderr)
        return out_dir / "RangeProbe/RangeProbe_8.cpfont"

    def glyph_metrics(self, cpfont: Path, codepoint: int):
        data = cpfont.read_bytes()
        _magic, _version, _flags, _styles, _reserved = struct.unpack_from("<8sHHB19s", data, 0)
        toc = struct.unpack_from("<B3xIIBhhHHBBBI4x", data, 32)
        interval_count, glyph_count, base = toc[1], toc[2], toc[11]
        for index in range(interval_count):
            first, last, offset = struct.unpack_from("<III", data, base + index * 12)
            if first <= codepoint <= last:
                glyph_index = offset + codepoint - first
                self.assertLess(glyph_index, glyph_count)
                width, height, _adv, left, top, _len, _off = struct.unpack_from(
                    "<BBHhhH2xI", data, base + interval_count * 12 + glyph_index * 16
                )
                return width, height, left, top
        self.fail(f"U+{codepoint:04X} missing from {cpfont.name}")

    def face_metrics(self, font: Path, codepoint: int, size: int = 8):
        import freetype

        face = freetype.Face(str(font))
        face.set_char_size(size << 6, size << 6, 150, 150)
        index = face.get_char_index(codepoint)
        self.assertNotEqual(index, 0, f"{font.name} lacks U+{codepoint:04X}")
        face.load_glyph(index, freetype.FT_LOAD_RENDER)
        bitmap = face.glyph.bitmap
        return bitmap.width, bitmap.rows, face.glyph.bitmap_left, face.glyph.bitmap_top

    def test_restricted_extra_fallback_leaves_symbols_to_the_builtin_stack(self):
        with tempfile.TemporaryDirectory() as work_dir:
            cpfont = self.build_probe(Path(work_dir), ["0x4E00-0x9FFF"])
            built = self.glyph_metrics(cpfont, self.MUSIC_NOTE)
        self.assertEqual(built, self.face_metrics(self.NOTO_SYMBOLS, self.MUSIC_NOTE))
        self.assertNotEqual(built, self.face_metrics(self.NOTO_CJK, self.MUSIC_NOTE))

    def test_unrestricted_extra_fallback_steals_the_symbol_glyph(self):
        """Shows the failure mode the ranges key prevents."""
        with tempfile.TemporaryDirectory() as work_dir:
            cpfont = self.build_probe(Path(work_dir), None)
            built = self.glyph_metrics(cpfont, self.MUSIC_NOTE)
        self.assertEqual(built, self.face_metrics(self.NOTO_CJK, self.MUSIC_NOTE))


class IntervalPresets(unittest.TestCase):
    def test_cjk_presets_resolve_to_the_documented_blocks(self):
        converter = load_module("fontconvert_sdcard.py", "fontconvert_sdcard")
        self.assertEqual(converter.INTERVAL_PRESETS["bopomofo"], [(0x3100, 0x312F)])
        self.assertEqual(converter.INTERVAL_PRESETS["cjk-ext-a"], [(0x3400, 0x4DBF)])

    def test_cjk_families_request_the_planned_intervals_and_sizes(self):
        for family in read_catalog():
            if family["name"] not in CJK_FAMILIES:
                continue
            with self.subTest(family=family["name"]):
                self.assertEqual(family["sizes"], [8, 10, 12, 14])
                self.assertEqual(
                    set(family["intervals"].split(",")),
                    {"cjk", "punctuation", "bopomofo", "cjk-ext-a"},
                )
                patched = sd_fonts.patched_intervals(family["intervals"]).split(",")
                self.assertIn("builtin", patched)


if __name__ == "__main__":
    unittest.main(verbosity=2)
