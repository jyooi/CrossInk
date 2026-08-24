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

A glyph that misses the arena now renders through the per-glyph SD overflow
path instead of a replacement box. This path is slow, since each miss opens
the font file and seeks and reads. Keep 16 pt out until device logs show
enough free heap, since a page with many missed glyphs still turns slowly.

A 2026-08-23 simulator baseline did not measure 16 pt heap.
The desktop simulator reports a fixed 1 MB free heap.
It cannot prove C3 headroom.
The new 1024-entry CJK advance cache adds up to 8 KB per style when full.
That extra resident cost makes 16 pt less safe, not more.
Do not add 16 pt files until an X4 log shows free heap after a dense 14 pt page.

### Cost of a missed glyph, per page turn

A grayscale page turn renders the page more than once. The reader draws it in
80-row strips, once per strip per plane, plus the black-and-white pass. A page
with more missed glyphs than the overflow ring holds evicts entries before a
later pass reaches them again.

The worst case per page turn is bounded by two terms:

- the missed glyphs on the page, past the ring's capacity
- one file open plus a seek and a read, for each of those

`GfxRenderer::textBaselineIntersectsStrip()` culls a whole run when its
baseline band misses the strip. That cull only helps the landscape
orientations. In Portrait and PortraitInverted, the reader defaults, a strip is
a slice of logical x, which every horizontal run crosses.

A run that survives the cull reaches the layout loop of
`GfxRenderer::drawText()`, which resolves every glyph of the run for its
advances. No per-glyph cull runs before that step. A page's distinct missed
glyphs stay resident in the ring across a page turn's passes as long as their
count does not exceed `OVERFLOW_CAPACITY`. Past that count, the ring evicts
and re-fetches, and a missed glyph can then cost more than one file open per
page turn. No device measurement of the per-open latency exists yet.

### Overflow ring capacity

`SdCardFont::OVERFLOW_CAPACITY` (`SdCardFont.h`) sizes the shared ring
described above. A 2026-08-24 simulator survey measured the worst page in
`test/epubs/test_chinese.epub` under both `LXGWWenKai` and `LXGWWenKaiTC` at
12 pt, over the whole book (30 simulated page-forward taps).

| Family | Worst page | Distinct off-arena glyphs |
| --- | --- | --- |
| LXGWWenKai (Simplified) | "注音与拼音" (Zhuyin and pinyin) chapter | 32 |
| LXGWWenKaiTC (Traditional) | same chapter, Traditional glyph forms | 32 |

The worst page is the same chapter in both scripts. Bopomofo marks and
pinyin tone diacritics sit outside the primary CJK Unified block. The
Traditional comparison chapter did not exceed this.

`OVERFLOW_CAPACITY` is 64, up from 8. That clears the measured worst case, 32,
with a full 32 slots of headroom for content this survey did not sample. 64
also keeps the count a round power of two.

RAM cost, computed for the ESP32-C3's 32-bit pointer ABI:

- `overflow_` is a member of `SdCardFont`.
  `SdCardFontManager` heap-allocates each `SdCardFont` with `new
  (std::nothrow)`. It is not a static or global array.
  `pio run -e default` reports the same static RAM before and after this
  change, 58052 bytes. Static RAM never held this array.
- `sizeof(OverflowEntry)` is 28 bytes: a 14-byte packed `EpdGlyph`, a 4-byte
  pointer, a 4-byte codepoint, and a 1-byte style index, with padding.
- Resident heap delta per loaded SD font instance: `(64 - 8) * 28` bytes,
  1568 bytes, about 1.53 KB.
  This is paid once per loaded family and point size, at font-load time.
  It is not paid per page turn.
  A book normally loads one SD font instance for body text.
  A second instance can load for the dictionary lookup font.
- Worst-case live bitmap heap, on top of the delta above: up to 64 slots
  can each hold one on-demand glyph bitmap at once.
  That is `OverflowEntry::bitmap`, freed only on eviction.
  This doc's own 12 pt density figure below is 28.2 KB over 224 glyphs,
  about 126 bytes per glyph.
  64 live slots at that average add up to about 8 KB of heap in the worst
  case.
  That worst case needs every ring slot full of a distinct glyph at once.
  The survey above did not observe that.
- Neither figure has a device measurement yet. See
  [Chinese EPUB Verification](chinese-epub-verification.md) for the pending
  checklist rows this change adds.

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

## UI text

Select the family in Font Options.
Then book titles, table of contents rows, and the reader header use Chinese glyphs.
A UI slot uses a fallback only when the family supplies that exact size.
Row heights and clipping come from the built-in UI font, so a different point size would overflow them.
A family without the 8, 10, or 12 pt file shows boxes in that slot.
A missing file does not add heap use.
The shipped LXGW WenKai families supply all three sizes.
If you do not select a CJK family, those strings stay as boxes.

The web portal unloads SD fonts to free heap.
Most portal exits restart the device, which reloads the family.
The exits that do not restart reload the family before the book list draws again.

Multi-line Chinese labels wrap between characters. The wrap uses the same no-break punctuation rules as the reader body, so a line does not start with a closing mark such as 。 or 》.
Latin labels still wrap on spaces.

## UI language

Settings, Language offers two entries at the end of the list, `简体中文 (zh-Hans)` for Simplified Chinese and
`繁體中文 (zh-Hant)` for Traditional Chinese.
The ASCII tag stays readable before a CJK UI font is installed, so you can tell the two rows apart.
Selecting one of these translates menus, buttons, and other UI text through the string tables in
`lib/I18n/translations/chinese-simplified.yaml` and `lib/I18n/translations/chinese-traditional.yaml`.

This UI language choice is separate from the reader font family.
The device still needs a CJK UI font family installed, such as `LXGWWenKai`, with its 8, 10, and 12 pt files present.
See [UI text](#ui-text). Without that family selected, Chinese UI labels render as boxes even when the UI language is Chinese.

File Transfer, Wi-Fi, Calibre, and OPDS show Chinese UI text as boxes while Wi-Fi is active, because the SD font unloads.

A key with no Chinese translation falls back to the English string rather than a box.

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
- The UI fallback needs the 8, 10, and 12 pt files. See [UI text](#ui-text) for the exact behavior.
- Do not ship a sparse GB2312 or Big5 subset. The converter starts a new interval at every missing codepoint. A sparse file can exceed `MAX_INTERVALS` (4096) and the firmware rejects it.
- Keep full CJK Unified and Extension A blocks so the interval table stays small.
- `MAX_PAGE_GLYPHS` stays at 512. A simulator run of Hongloumeng at 12 pt used 224 unique glyphs and 28.2 KB on the densest page. The synthetic test book peaked at 199 glyphs and 23.9 KB. The X4 heap floor is still unmeasured. Do not raise the cap until a device log shows a page that hits 512.
- One of the 512 slots holds the replacement glyph, so a page can carry 511 unique text glyphs. A page that needs more logs once. The extra glyphs still draw correctly through the slow per-glyph SD overflow path.
- CJK families use a 1024-entry advance cache. Latin families stay at 256. Each entry is 8 bytes and grows on demand. A 1024-entry table is 8 KB per style when full.

## Advance cache evidence

Measurements come from one simulator run of Hongloumeng with the WenKai family at 12 pt.
The same two dense sections were measured before and after the change from 256 to 1024 entries.

| Section | Unique codepoints | Layout time before | Cache resets before | Layout time after | Cache resets after |
| --- | --- | --- | --- | --- | --- |
| A | 556 | 1 ms | 5 | 1 ms | 0 |
| B | 890 | 2 ms | 8 | 2 ms | 0 |

The reset counts are the `reset full cache` lines that follow each section
prewarm line in the same run. The whole before run logged 19 of them. The after
run logged 0.

The millisecond times do not change, because the simulator reads the font file through POSIX I/O.
POSIX I/O hides the per-character SD cost that the device pays.
The reset count is therefore the useful signal.
A reset drops the whole table, so every later character reopens the font file on real hardware.
The 1024-entry limit removes every reset on both sections.

RAM cost of the change:

- Static RAM grows by 5 bytes for each `SdCardFont` instance.
- Heap grows on demand to 8 KB for each style at 1024 entries, against 2 KB at 256 entries.
- The table is per style, and a page scan can ask for all 4 styles. The worst
  case therefore rises from 4 x 2 KB = 8 KB to 4 x 8 KB = 32 KB of heap, a delta
  of plus 24 KB against about 380 KB of usable internal RAM on the C3.
- `pio run -e default` reports RAM 58036 / 327680, which is 17.7 percent. That
  figure covers static RAM only. It excludes the on-demand advance-table heap
  above.
- A merge holds three buffers at once: the scratch, up to 8 KB, the replacement
  table `ensureAdvanceTableCapacity` allocates, up to 8 KB, and the old table it
  has not released yet, up to 4 KB. The transient peak is therefore about 20 KB,
  and it needs two separate 8 KB contiguous blocks. The old 256-entry limit
  needed about 5 KB in total. The contiguous size is what fails first on a
  fragmented heap.
- Phase 4 must check the 4-style aggregate and this transient peak on a device.
  Record free heap and largest allocatable block after a dense 12 pt page and a
  dense 14 pt page. See [Chinese EPUB Verification](chinese-epub-verification.md)
  for the checklist. A 2026-08-24 SD-card install on the Xteink X4 confirmed
  the feature renders correctly on hardware (the USB path on this host still
  fails to enumerate). That pass did not capture heap numbers, so this check
  is still open.

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
| `PGTURN prewarm=...ms bw=...ms gray=...ms total=...ms` | `LOG_DBG` | no |
| `PGMISS overflow SD open cp=U+...` | `LOG_DBG` | no |

`PGTURN` logs once per page turn, from `EpubReaderActivity::renderContents`.
It times the prewarm phase, the black-and-white pass, and the grayscale
passes, so a device log can see where a slow turn spent its time.

`PGMISS` logs once per real SD open in `SdCardFont::onGlyphMiss`, skipping
ring hits. On device, count `PGMISS` lines within one page turn and compare
against the page's distinct missed codepoints. A count above that number
means the overflow ring thrashed on that page.

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
