#!/usr/bin/env python3
"""Regression check: the SD glyph-miss overflow ring must not thrash.

Runs the simulator against a Chinese EPUB with a CJK SD font, then reads
the `PGMISS` and `PGTURN` debug log lines that
`EpubReaderActivity::renderContents` and `SdCardFont::onGlyphMiss` emit.
For every page turn, this asserts that the count of real SD opens
(`PGMISS` lines) equals the count of distinct missed codepoints on that
page. A higher count means the overflow ring evicted and re-fetched the
same codepoint, the thrash described in
docs/chinese-fonts.md#overflow-ring-capacity. The run also fails when the
SD font family never loaded, when no `PGTURN` line appears, or when no
page turn touches the overflow ring.

This reuses `run_simulator_smoke_test.py`'s fs_ staging and simulator
invocation, so it needs the same isolated `.pio/build/simulator/program`
binary, built with `-DLOG_LEVEL=2` (the `simulator` env sets this by
default).
"""

from __future__ import annotations

import argparse
import os
import re
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from run_simulator_smoke_test import (  # noqa: E402
    build_simulator,
    font_load_error,
    install_font,
    prepare_fs,
    program_path,
)

import subprocess

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_BOOK = ROOT / "test" / "epubs" / "test_chinese.epub"

PGTURN_RE = re.compile(r"PGTURN .*total=(\d+)ms")
PGMISS_RE = re.compile(r"PGMISS overflow SD open cp=(U\+[0-9A-F]+) style=(\d+)")


def bucket_misses_per_turn(stdout: str) -> list[list[str]]:
    """Group each PGTURN line with the PGMISS lines that came before it."""
    buckets: list[list[str]] = []
    current: list[str] = []
    for line in stdout.splitlines():
        miss = PGMISS_RE.search(line)
        if miss:
            current.append(f"{miss.group(1)}:{miss.group(2)}")
            continue
        if PGTURN_RE.search(line):
            buckets.append(current)
            current = []
    return buckets


def run_simulator(font_dir: Path, family: str, page_turns: int, env_name: str, timeout: int, build: bool) -> str:
    if build:
        build_simulator(env_name)

    program = program_path(env_name)
    if not program.exists():
        raise SystemExit(f"Simulator binary not found: {program}. Run: pio run -e {env_name}")

    with tempfile.TemporaryDirectory(prefix="crossink-overflow-ring-") as temp_dir_name:
        temp_root = Path(temp_dir_name)
        simulator_book_path = prepare_fs(temp_root, DEFAULT_BOOK)
        install_font(temp_root, font_dir, family)

        env = os.environ.copy()
        env["CROSSINK_SIMULATOR_SMOKE_TEST"] = "1"
        env["CROSSINK_SIMULATOR_SMOKE_BOOK"] = simulator_book_path
        env["CROSSINK_SIMULATOR_SMOKE_PAGE_TURNS"] = str(page_turns)
        env.setdefault("CROSSINK_SIM_SD_FONT", family)
        env.setdefault("SDL_VIDEODRIVER", "dummy")

        proc = subprocess.run(
            [str(program)],
            cwd=temp_root,
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
        )
    if proc.returncode != 0:
        raise SystemExit(f"Simulator exited {proc.returncode}:\n{proc.stdout}")
    return proc.stdout


def check(stdout: str, family: str) -> int:
    """Return a process exit code for one simulator log."""
    error = font_load_error(stdout, family)
    if error:
        print(f"FAIL: {error}", file=sys.stderr)
        return 1
    if not PGTURN_RE.search(stdout):
        print("FAIL: simulator produced no PGTURN line.", file=sys.stderr)
        return 1

    thrashed = 0
    checked_pages = 0
    for misses in bucket_misses_per_turn(stdout):
        if not misses:
            continue
        checked_pages += 1
        unique = len(set(misses))
        raw = len(misses)
        if raw != unique:
            thrashed += 1
            print(f"THRASH: page had {unique} distinct missed codepoints but {raw} SD opens", file=sys.stderr)
    print(f"Checked {checked_pages} page turn(s) with at least one overflow-ring miss.")
    if checked_pages == 0:
        print("FAIL: no page turn touched the overflow ring.", file=sys.stderr)
        return 1
    if thrashed:
        print(f"FAIL: {thrashed} page turn(s) thrashed the SD glyph-miss overflow ring.", file=sys.stderr)
        return 1
    print("PASS: no page turn re-fetched an already-missed codepoint.")
    return 0


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--font-dir", required=True, help="Built .cpfont family folder, e.g. LXGWWenKai")
    parser.add_argument("--font-family", default="LXGWWenKai", help="Family name to install under (default: LXGWWenKai)")
    parser.add_argument("--env", default="simulator", help="PlatformIO simulator environment")
    parser.add_argument("--page-turns", type=int, default=30, help="Page-forward taps (30 covers the whole fixture)")
    parser.add_argument("--timeout", type=int, default=90)
    parser.add_argument("--no-build", dest="build", action="store_false")
    parser.set_defaults(build=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    stdout = run_simulator(
        Path(args.font_dir).resolve(), args.font_family, args.page_turns, args.env, args.timeout, args.build
    )
    return check(stdout, args.font_family)


if __name__ == "__main__":
    raise SystemExit(main())
