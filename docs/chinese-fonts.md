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
falls below 40 KB. This keeps working heap for kerning data and for the render
pass. A 16 pt page can still exceed that reserve or the per-style chunk
ceiling.

Any glyph that misses the arena draws blank today, not as a correct character.
The arena reads glyphs in file-offset order, and U+FFFD holds the highest offset,
so an early stop drops the replacement glyph first. The renderer looks glyphs up
through a path that does not reach the per-glyph SD fallback. A separate fix must
land first.
Keep 16 pt out until device logs show enough free heap.

A 2026-08-23 simulator baseline did not measure 16 pt heap.
The desktop simulator reports a fixed 1 MB free heap.
It cannot prove C3 headroom.
The new 1024-entry CJK advance cache adds up to 8 KB per style when full.
That extra resident cost makes 16 pt less safe, not more.
Do not add 16 pt files until an X4 log shows free heap after a dense 14 pt page.

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
- `MAX_PAGE_GLYPHS` stays at 512. A simulator run of Hongloumeng at 12 pt used 224 unique glyphs and 28.2 KB on the densest page. The synthetic test book peaked at 199 glyphs and 23.9 KB. The X4 heap floor is still unmeasured. Do not raise the cap until a device log shows a page that hits 512.
- One of the 512 slots holds the replacement glyph, so a page can carry 511 unique text glyphs. A page that needs more logs once. Slot 0 only puts U+FFFD in the codepoint list. It does not promise that the U+FFFD bitmap reaches the arena, so the extra glyphs draw as boxes while U+FFFD stays resident, and blank after the arena drops it.
- CJK families use a 1024-entry advance cache. Latin families stay at 256. Each entry is 8 bytes and grows on demand. A 1024-entry table is 8 KB per style when full.

## Advance cache evidence

Measurements come from one simulator run of Hongloumeng with the WenKai family at 12 pt.
The same two dense sections were measured before and after the change from 256 to 1024 entries.

| Section | Unique codepoints | Layout time before | Cache resets before | Layout time after | Cache resets after |
| --- | --- | --- | --- | --- | --- |
| A | 556 | 1 ms | several | 1 ms | 0 |
| B | 890 | 2 ms | several | 2 ms | 0 |

The millisecond times do not change, because the simulator reads the font file through POSIX I/O.
POSIX I/O hides the per-character SD cost that the device pays.
The reset count is therefore the useful signal.
A reset drops the whole table, so every later character reopens the font file on real hardware.
The 1024-entry limit removes every reset on both sections.

RAM cost of the change:

- Static RAM grows by 5 bytes for each `SdCardFont` instance.
- Heap grows on demand to 8 KB for each style at 1024 entries, against 2 KB at 256 entries.
- `pio run -e default` reports RAM 58036 / 327680, which is 17.7 percent.

## Log visibility during a device check

Some lines that a hardware check needs are compiled out of the `default` build.
`env:default` sets `-DLOG_LEVEL=1`, and `LOG_DBG` needs `LOG_LEVEL >= 2`.
Flash `pio run -e debug`, which sets `-DLOG_LEVEL=2`, to see every line below.

| Log line | Level | Visible on `env:default` |
| --- | --- | --- |
| `Advance cache limit ... entries` | `LOG_INF` | yes |
| `Page glyph cap 512 hit` | `LOG_ERR` | yes |
| `Advance table style N: reset full cache` | `LOG_DBG` | no |
| `[page] total=...ms sd_read=...ms` from `logStats` | `LOG_DBG` | no |

The absence of a reset line on an `env:default` build proves nothing.
Use a `-DLOG_LEVEL=2` build before you claim that the resets are gone.

## Anti-aliasing on Chinese pages

Text anti-aliasing draws each page three times.
One pass is black and white. Two passes build the gray overlay.
The X4 device cost of those gray passes is still unmeasured.
This change adds no new setting.

Use Settings, Reader, Text Anti-Aliasing if a Chinese page turn is too slow.
Turn the option off to skip the gray passes.
Leave the option on for smoother glyph edges.

## Licenses

LXGW WenKai, LXGW WenKai TC, and Noto Sans CJK SC use the SIL Open Font License.
You may build and copy the `.cpfont` files for personal use.
Keep the OFL notice if you publish the built files.
