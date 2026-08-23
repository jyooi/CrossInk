---
title: Chinese Fonts
nav_order: 4.5
---

# Chinese Fonts

Use these SD-card families for Simplified and Traditional Chinese books.
The firmware does not ship a CJK font. You must build the files and copy them to the SD card.

## Which family to use

| Book script | Family | Glyph forms |
| --- | --- | --- |
| Simplified Chinese | `LXGWWenKai` | Simplified |
| Traditional Chinese | `LXGWWenKaiTC` | Traditional |

Select the family in Settings, Reader, Font Options, Font Family.
The size list shows only the files that exist. These families ship 8, 10, 12, and 14 pt.

## Why the size cap is 14 pt

A dense Chinese page at 12 pt needs about 45-50 KB for glyph bitmaps.
The same page at 16 pt needs about 85-110 KB. The Xteink X4 has no PSRAM.

The glyph arena assembles from 4 KB chunks, so a single fragmented block no
longer drops a whole page. The arena stops taking chunks before free heap
falls below 40 KB or the largest free block falls below 32 KB. This keeps
working heap for kerning data and for the render pass. A 16 pt page can still
exceed that reserve or the per-style chunk ceiling. Any glyph that misses the
arena loads one by one instead.
Keep 16 pt out until device logs show enough free heap.

## How to build the files

Install the Python tools in a local venv. Do not install them for the whole system.

    python3 -m venv .venv-fonts
    .venv-fonts/bin/pip install -r lib/EpdFont/scripts/requirements.txt

Build both families from a clean checkout:

    .venv-fonts/bin/python lib/EpdFont/scripts/build-cjk-fonts.py \
      --output-dir lib/EpdFont/scripts/output/cjk \
      --clean \
      --verbose

Build one family only:

    .venv-fonts/bin/python lib/EpdFont/scripts/build-cjk-fonts.py \
      --only LXGWWenKai \
      --output-dir lib/EpdFont/scripts/output/cjk

Use local TTF copies if you already have the v1.522 files:

    .venv-fonts/bin/python lib/EpdFont/scripts/build-cjk-fonts.py \
      --local-dir /path/to/wenkai-ttf \
      --output-dir lib/EpdFont/scripts/output/cjk

The script pins LXGW WenKai v1.522 and LXGW WenKai TC v1.522.
It downloads those files into `lib/EpdFont/scripts/downloaded_fonts/`.
Git ignores that folder. Do not commit TTF or `.cpfont` files.

Inspect the output:

    .venv-fonts/bin/python lib/EpdFont/scripts/inspect_cpfont.py \
      --require-cjk-contiguous \
      lib/EpdFont/scripts/output/cjk

Each style must keep 4096 intervals or fewer. That is the firmware `MAX_INTERVALS` limit.

## SD card layout

Copy each family folder to the hidden fonts root. The visible `/fonts/` root also works.

    SD Card Root/
    └── .fonts/
        ├── LXGWWenKai/
        │   ├── LXGWWenKai_8.cpfont
        │   ├── LXGWWenKai_10.cpfont
        │   ├── LXGWWenKai_12.cpfont
        │   └── LXGWWenKai_14.cpfont
        └── LXGWWenKaiTC/
            ├── LXGWWenKaiTC_8.cpfont
            ├── LXGWWenKaiTC_10.cpfont
            ├── LXGWWenKaiTC_12.cpfont
            └── LXGWWenKaiTC_14.cpfont

Each `.cpfont` file holds regular and bold in one size.
The firmware reads `/.fonts/<Family>/<Family>_<size>.cpfont`.
The 8, 10, and 12 pt files also feed the CJK UI fallback for titles and menus.

## Web upload path

1. Start File Transfer.
2. Open the web URL on the device screen.
3. Open the Fonts tab.
4. Upload the `.cpfont` files for one family name.

You can also copy the family folder onto the SD card on a computer. That path is faster for a full family.

These families are not in the on-device download catalog. The catalog uses a fixed remote list.

## Build assumptions

- Bold uses LXGW WenKai Medium. The family has no true Bold face in the pinned release.
- `LXGWWenKaiTC` uses WenKai SC as the first fallback for Han codepoints that WenKai TC lacks.
- Noto Sans CJK SC is the last CJK fallback. The file is already in the repo.
- The CJK fallbacks only cover CJK blocks. The shared reader fallbacks still supply the Latin, Cyrillic, symbol, and emoji glyphs.
- Intervals are `builtin`, `cjk`, `punctuation`, `bopomofo`, and `cjk-ext-a`.
- The converter also adds U+FFFD.

## Known limits

- There is no italic CJK face. The firmware uses the closest present style.
- The UI fallback needs the 8, 10, and 12 pt files. A later phase still has to prove that path on hardware.
- Do not ship a sparse GB2312 or Big5 subset. The converter starts a new interval at every missing codepoint. A sparse file can exceed `MAX_INTERVALS` (4096) and the firmware rejects it.
- Keep full CJK Unified and Extension A blocks so the interval table stays small.

## Licenses

LXGW WenKai, LXGW WenKai TC, and Noto Sans CJK SC use the SIL Open Font License.
You may build and copy the `.cpfont` files for personal use.
Keep the OFL notice if you publish the built files.
