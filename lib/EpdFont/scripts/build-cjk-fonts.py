#!/usr/bin/env python3
"""Build LXGW WenKai SD-card families for Simplified and Traditional Chinese.

This wrapper keeps the family catalog in ``sd-fonts.yaml`` and delegates to
``build-sd-fonts.py``. The CJK families are marked ``optional`` so the default
font-release workflow does not download them.

Pinned sources:
    LXGW WenKai v1.522 Regular + Medium (Medium is used as bold)
    LXGW WenKai TC v1.522 Regular + Medium
    Noto Sans CJK SC Regular from the repo (last fallback)

Usage:
    python3 lib/EpdFont/scripts/build-cjk-fonts.py
    python3 lib/EpdFont/scripts/build-cjk-fonts.py --only LXGWWenKai
    python3 lib/EpdFont/scripts/build-cjk-fonts.py --output-dir ./generated-cjk-fonts
    python3 lib/EpdFont/scripts/build-cjk-fonts.py --local-dir /path/to/ttfs
"""

from __future__ import annotations

import argparse
import copy
import subprocess
import sys
from pathlib import Path

import yaml


SCRIPT_DIR = Path(__file__).resolve().parent
SHARED_BUILDER = SCRIPT_DIR / "build-sd-fonts.py"
DEFAULT_CONFIG = SCRIPT_DIR / "sd-fonts.yaml"
PROJECT_ROOT = SCRIPT_DIR.parents[2]
DEFAULT_OUTPUT = PROJECT_ROOT / "lib/EpdFont/scripts/output/cjk"
CJK_FAMILY_NAMES = ("LXGWWenKai", "LXGWWenKaiTC")
NOTO_CJK_SC = (
    PROJECT_ROOT / "lib/EpdFont/builtinFonts/source/NotoSansCJKsc/NotoSansCJKsc-Regular.otf"
)
LOCAL_FILE_ALIASES = {
    "LXGWWenKai-Regular.ttf": ("LXGWWenKai-Regular.ttf",),
    "LXGWWenKai-Medium.ttf": ("LXGWWenKai-Medium.ttf",),
    "LXGWWenKaiTC-Regular.ttf": ("LXGWWenKaiTC-Regular.ttf",),
    "LXGWWenKaiTC-Medium.ttf": ("LXGWWenKaiTC-Medium.ttf",),
}


def load_cjk_config(config_path: Path) -> tuple[dict, list[dict]]:
    """Load the catalog and keep only the CJK families."""
    with config_path.open() as config_file:
        config = yaml.safe_load(config_file)

    if not isinstance(config, dict) or not config.get("families"):
        raise ValueError(f"No families defined in {config_path}")

    cjk_config = copy.deepcopy(config)
    selected = [
        family
        for family in cjk_config["families"]
        if family.get("name") in CJK_FAMILY_NAMES
    ]
    missing = set(CJK_FAMILY_NAMES) - {family.get("name") for family in selected}
    if missing:
        raise ValueError(f"CJK families missing from {config_path}: {', '.join(sorted(missing))}")

    cjk_config["families"] = selected
    return cjk_config, selected


def apply_local_sources(families: list[dict], local_dir: Path) -> None:
    """Replace download URLs with files from a local directory."""
    available = {path.name: path for path in local_dir.iterdir() if path.is_file()}

    def resolve_named(filename: str) -> Path:
        for alias in LOCAL_FILE_ALIASES.get(filename, (filename,)):
            if alias in available:
                return available[alias]
        raise FileNotFoundError(f"{local_dir} does not contain {filename}")

    def rewrite_spec(spec: dict) -> None:
        if "url" not in spec:
            return
        filename = spec["url"].rsplit("/", 1)[-1]
        local_path = resolve_named(filename).resolve()
        spec.pop("url")
        epdfonts_dir = PROJECT_ROOT / "lib/EpdFont"
        try:
            spec["path"] = str(local_path.relative_to(epdfonts_dir))
        except ValueError:
            spec["path"] = str(local_path)

    for family in families:
        for style_spec in family.get("styles", {}).values():
            rewrite_spec(style_spec)
        for key, value in list(family.items()):
            if key == "extra_fallbacks" or key.startswith("extra_fallbacks_"):
                for spec in value or []:
                    rewrite_spec(spec)


def write_temporary_config(config: dict) -> Path:
    """Write a temporary catalog for the shared builder."""
    import tempfile

    temp_file = tempfile.NamedTemporaryFile(
        mode="w", prefix="crossink-cjk-fonts-", suffix=".yaml", delete=False
    )
    try:
        yaml.safe_dump(config, temp_file, sort_keys=False)
    finally:
        temp_file.close()
    return Path(temp_file.name)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Build LXGW WenKai CJK SD-card fonts from sd-fonts.yaml"
    )
    parser.add_argument(
        "--config", default=str(DEFAULT_CONFIG), help="Font catalog YAML (default: sd-fonts.yaml)"
    )
    parser.add_argument(
        "--output-dir",
        default=str(DEFAULT_OUTPUT),
        help="Output directory (default: lib/EpdFont/scripts/output/cjk)",
    )
    parser.add_argument(
        "--only",
        help="Comma-separated family names (default: LXGWWenKai,LXGWWenKaiTC)",
    )
    parser.add_argument(
        "--local-dir",
        help="Directory with pinned WenKai TTF files. Skips the download.",
    )
    parser.add_argument("--jobs", "-j", type=int, default=1, help="Maximum parallel families")
    parser.add_argument(
        "--timeout",
        type=int,
        default=7200,
        help="Per-family timeout in seconds (default: 7200)",
    )
    parser.add_argument("--clean", action="store_true", help="Clean the output directory first")
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Stream child font-converter output"
    )
    parser.add_argument(
        "--package",
        action="store_true",
        help="Also write per-family ZIP archives",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    config_path = Path(args.config).expanduser().resolve()
    output_dir = Path(args.output_dir).expanduser().resolve()

    if not SHARED_BUILDER.is_file():
        print(f"ERROR: shared builder not found: {SHARED_BUILDER}", file=sys.stderr)
        return 1
    if not NOTO_CJK_SC.is_file():
        print(f"ERROR: Noto Sans CJK SC not found: {NOTO_CJK_SC}", file=sys.stderr)
        return 1
    if not config_path.is_file():
        print(f"ERROR: config not found: {config_path}", file=sys.stderr)
        return 1

    try:
        cjk_config, families = load_cjk_config(config_path)
    except (OSError, ValueError, yaml.YAMLError) as error:
        print(f"ERROR: unable to load CJK config: {error}", file=sys.stderr)
        return 1

    if args.only:
        requested = {name.strip() for name in args.only.split(",") if name.strip()}
        families = [family for family in families if family.get("name") in requested]
        if not families:
            print(f"ERROR: no matching families for --only {args.only}", file=sys.stderr)
            return 1
        cjk_config["families"] = families

    if args.local_dir:
        local_dir = Path(args.local_dir).expanduser().resolve()
        if not local_dir.is_dir():
            print(f"ERROR: local directory not found: {local_dir}", file=sys.stderr)
            return 1
        try:
            apply_local_sources(families, local_dir)
        except (FileNotFoundError, ValueError) as error:
            print(f"ERROR: {error}", file=sys.stderr)
            return 1

    temp_config = write_temporary_config(cjk_config)
    command = [
        sys.executable,
        str(SHARED_BUILDER),
        "--config",
        str(temp_config),
        "--output-dir",
        str(output_dir),
        "--only",
        ",".join(family["name"] for family in families),
        "--timeout",
        str(args.timeout),
        "--jobs",
        str(args.jobs),
    ]
    if not args.package:
        command.append("--no-package")
    if args.clean:
        command.append("--clean")
    if args.verbose:
        command.append("--verbose")

    print("CJK families: " + ", ".join(family["name"] for family in families))
    print("Pinned release: LXGW WenKai / WenKai TC v1.522")
    print(f"Noto Sans CJK SC fallback: {NOTO_CJK_SC}")
    print("Bold source: LXGW WenKai Medium (Medium-as-bold)")
    try:
        result = subprocess.run(command, check=False)
    finally:
        temp_config.unlink(missing_ok=True)

    if result.returncode != 0:
        return result.returncode

    inspect = SCRIPT_DIR / "inspect_cpfont.py"
    if inspect.is_file():
        inspect_cmd = [
            sys.executable,
            str(inspect),
            "--require-cjk-contiguous",
            str(output_dir),
        ]
        inspect_result = subprocess.run(inspect_cmd, check=False)
        if inspect_result.returncode != 0:
            return inspect_result.returncode
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
