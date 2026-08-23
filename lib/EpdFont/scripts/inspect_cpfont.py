#!/usr/bin/env python3
"""Parse .cpfont headers and report glyph counts, intervals, and file size.

The firmware rejects a style whose intervalCount is greater than 4096
(see SdCardFont.cpp MAX_INTERVALS). This tool also checks that CJK Unified
Ideographs and Extension A stay in contiguous intervals after gap filling.
"""

from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from cpfont_version import CPFONT_VERSION


MAGIC = b"CPFONT\x00\x00"
HEADER_SIZE = 32
STYLE_TOC_ENTRY_SIZE = 32
STYLE_TOC_FORMAT = "<B3xIIBhhHHBBBI4x"
INTERVAL_FORMAT = "<III"
MAX_INTERVALS = 4096
CJK_UNIFIED = (0x4E00, 0x9FFF)
CJK_EXT_A = (0x3400, 0x4DBF)
STYLE_NAMES = {0: "regular", 1: "bold", 2: "italic", 3: "bolditalic"}


def read_cpfont(path: Path) -> dict:
    data = path.read_bytes()
    if len(data) < HEADER_SIZE:
        raise ValueError(f"{path.name}: file too small for header")

    magic, version, flags, style_count, _reserved = struct.unpack_from("<8sHHB19s", data, 0)
    if magic != MAGIC:
        raise ValueError(f"{path.name}: invalid magic {magic!r}")
    if version != CPFONT_VERSION:
        raise ValueError(f"{path.name}: unsupported version {version} (expected {CPFONT_VERSION})")
    if style_count == 0:
        raise ValueError(f"{path.name}: style count is 0")

    styles = []
    for index in range(style_count):
        toc_offset = HEADER_SIZE + index * STYLE_TOC_ENTRY_SIZE
        if toc_offset + STYLE_TOC_ENTRY_SIZE > len(data):
            raise ValueError(f"{path.name}: truncated style TOC")
        unpacked = struct.unpack_from(STYLE_TOC_FORMAT, data, toc_offset)
        style_id = unpacked[0]
        interval_count = unpacked[1]
        glyph_count = unpacked[2]
        data_offset = unpacked[11]
        intervals = []
        cursor = data_offset
        for _ in range(interval_count):
            if cursor + 12 > len(data):
                raise ValueError(f"{path.name}: truncated interval table")
            first, last, offset = struct.unpack_from(INTERVAL_FORMAT, data, cursor)
            intervals.append((first, last, offset))
            cursor += 12
        styles.append(
            {
                "id": style_id,
                "name": STYLE_NAMES.get(style_id, str(style_id)),
                "interval_count": interval_count,
                "glyph_count": glyph_count,
                "intervals": intervals,
            }
        )
    return {
        "path": path,
        "version": version,
        "flags": flags,
        "style_count": style_count,
        "size": path.stat().st_size,
        "styles": styles,
    }


def covers_contiguous(intervals: list[tuple[int, int, int]], start: int, end: int) -> bool:
    """Return True when [start, end] sits inside one interval."""
    return any(first <= start and last >= end for first, last, _offset in intervals)


def format_table(reports: list[dict]) -> str:
    lines = [
        f"{'file':<36} {'style':<12} {'glyphs':>7} {'intervals':>10} {'size_mb':>8} {'ok':>4}"
    ]
    for report in reports:
        size_mb = report["size"] / 1024 / 1024
        for style in report["styles"]:
            ok = "yes" if style["interval_count"] <= MAX_INTERVALS else "NO"
            lines.append(
                f"{report['path'].name:<36} {style['name']:<12} "
                f"{style['glyph_count']:>7} {style['interval_count']:>10} "
                f"{size_mb:>8.2f} {ok:>4}"
            )
    return "\n".join(lines)


def collect_paths(targets: list[Path]) -> list[Path]:
    files: list[Path] = []
    for target in targets:
        if target.is_file() and target.suffix == ".cpfont":
            files.append(target)
        elif target.is_dir():
            files.extend(sorted(target.rglob("*.cpfont")))
        else:
            raise FileNotFoundError(target)
    return files


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Inspect .cpfont headers and interval tables")
    parser.add_argument("paths", nargs="+", help="cpfont files or directories")
    parser.add_argument(
        "--require-cjk-contiguous",
        action="store_true",
        help="Fail if CJK Unified or Ext A is split across more than one interval",
    )
    parser.add_argument(
        "--max-intervals",
        type=int,
        default=MAX_INTERVALS,
        help=f"Fail when a style exceeds this interval count (default: {MAX_INTERVALS})",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        files = collect_paths([Path(item) for item in args.paths])
    except FileNotFoundError as error:
        print(f"ERROR: not found: {error}", file=sys.stderr)
        return 1
    if not files:
        print("ERROR: no .cpfont files found", file=sys.stderr)
        return 1

    reports = []
    failed = False
    for path in files:
        try:
            report = read_cpfont(path)
        except (OSError, ValueError, struct.error) as error:
            print(f"ERROR: {error}", file=sys.stderr)
            failed = True
            continue
        reports.append(report)
        for style in report["styles"]:
            if style["interval_count"] > args.max_intervals:
                print(
                    f"ERROR: {path.name} {style['name']}: "
                    f"{style['interval_count']} intervals exceeds {args.max_intervals}",
                    file=sys.stderr,
                )
                failed = True
            if args.require_cjk_contiguous:
                if not covers_contiguous(style["intervals"], *CJK_UNIFIED):
                    print(
                        f"ERROR: {path.name} {style['name']}: CJK Unified is not contiguous",
                        file=sys.stderr,
                    )
                    failed = True
                if not covers_contiguous(style["intervals"], *CJK_EXT_A):
                    print(
                        f"ERROR: {path.name} {style['name']}: CJK Ext A is not contiguous",
                        file=sys.stderr,
                    )
                    failed = True

    print(format_table(reports))
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
