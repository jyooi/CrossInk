---
title: Chinese EPUB Verification
nav_order: 4.6
---

# Chinese EPUB Verification

This is the hardware checklist for the Chinese EPUB feature set.
That set covers CJK SD-card fonts, CJK line-break and indent typography, and
the chunked glyph arena and its degrade path.
It also covers the CJK UI fallback and wrapping.
Run it on the target device, the Xteink X4 (ESP32-C3, no PSRAM).
See [Chinese Fonts](chinese-fonts.md) for font family and size background.

## Prerequisites

- Build `LXGWWenKai` at 8, 10, 12, and 14 pt with `lib/EpdFont/scripts/build-cjk-fonts.py`.
  Copy the family folder to `/.fonts/LXGWWenKai/` on the SD card.
- Copy a Simplified Chinese EPUB onto the SD card. Use a dense book, not only
  the synthetic `test/epubs/test_chinese.epub` fixture, so glyph-cap and
  arena-degrade paths get real coverage.
- Flash `pio run -e debug` instead of `pio run -e default` if you need the
  `LOG_DBG` lines listed in [Chinese Fonts](chinese-fonts.md#log-visibility-during-a-device-check).
  `env:default` sets `LOG_LEVEL=1`, so `LOG_DBG` lines do not print there.

## Checklist

Record free heap and max allocatable block with `ESP.getFreeHeap()` and
`ESP.getMaxAllocHeap()` at every heap-column row.
Note the largest allocatable block too.
A fragmented heap can fail before free heap runs out.

| # | Step | Expected result | Result |
| - | --- | --- | --- |
| 1 | Cold boot | Device boots to Home without a crash or reset loop | Holistic pass, 2026-08-24 (see notes below) |
| 2 | Open the Chinese book from Home | Title renders in Chinese via the CJK UI fallback, no boxes | Holistic pass, 2026-08-24 (see notes below) |
| 3 | Heap after open | Record free heap / max alloc | Pending, no heap figures reported |
| 4 | 20 page turns, Text Anti-Aliasing on | Every page renders, no boxes, no crash, page-turn time feels consistent | Holistic pass, 2026-08-24 (see notes below) |
| 5 | Heap after AA-on turns | Record free heap / max alloc | Pending, no heap figures reported |
| 6 | Settings, Reader, Text Anti-Aliasing off | Setting takes effect immediately | Not exercised in the 2026-08-24 pass |
| 7 | 20 page turns, Text Anti-Aliasing off | Every page renders, turns are faster than step 4 | Not exercised in the 2026-08-24 pass |
| 8 | Heap after AA-off turns | Record free heap / max alloc | Pending, no heap figures reported |
| 9 | Open Table of Contents | Chinese chapter titles render, wrap correctly between characters, no boxes | Holistic pass, 2026-08-24 (see notes below) |
| 10 | Return to Home, check Recent Books | Chinese title renders correctly in the Recent Books row | Holistic pass, 2026-08-24 (see notes below) |
| 11 | Sleep, then wake | Device wakes to the same page, no crash, no stale render | Holistic pass, 2026-08-24 (see notes below) |
| 12 | Heap after sleep/wake | Record free heap / max alloc | Pending, no heap figures reported |
| 13 | Start Wi-Fi file transfer, then exit | Web portal opens; after exit the CJK UI fallback still shows Chinese titles without a restart | Not exercised in the 2026-08-24 pass |
| 14 | Heap after Wi-Fi transfer round trip | Record free heap / max alloc, compare against step 3 for a leak | Pending, no heap figures reported |
| 15 | Dense chapter page (highest measured glyph count) | Page renders correctly; a glyph that misses the arena still draws through the slow per-glyph path instead of a box | Holistic pass, 2026-08-24 (see notes below) |
| 16 | Flash `pio run -e debug`. Turn 20 pages on an English EPUB, then 20 pages on this Chinese EPUB | Record every `PGTURN prewarm=...ms bw=...ms gray=...ms total=...ms` line for both runs | Pending, no device log captured |
| 17 | During the Chinese run in row 16, on the pinyin and Zhuyin chapter | Count `PGMISS` lines per page turn against that page's distinct missed codepoints, per [Chinese Fonts](chinese-fonts.md#log-visibility-during-a-device-check) | Pending, no device log captured |
| 18 | Heap after the dense 14 pt page from row 15 | Record free heap / max alloc via `ESP.getFreeHeap()` and `ESP.getMaxAllocHeap()` | Pending, no heap figures reported |

Rows 16 to 18 are new with the `OVERFLOW_CAPACITY` change in [Chinese
Fonts](chinese-fonts.md#overflow-ring-capacity).
They need a real device log.
The simulator cannot produce them, because its SD reads and heap figures are
not real, as explained there.

## Known unmeasured risk

[Chinese Fonts](chinese-fonts.md#advance-cache-evidence) flags two device
numbers this checklist should capture.
No earlier phase measured them:

- The 4-style advance-cache aggregate (up to 32 KB resident when a page scan
  touches all four styles at the 1024-entry cap).
- The transient merge peak (about 20 KB across two separate 8 KB contiguous
  blocks) when the advance table grows.

Steps 3, 5, 8, 12, and 14 above are the points to capture free heap and max
allocatable block for this risk.
A device log at `-DLOG_LEVEL=2` also shows two useful lines:
`Advance cache limit ... entries` per style, and any `reset full cache` line.
A reset full cache line means the cache cleared.
The 1024-entry cap did not help on that page.

## Verification run log

| Date | Device | Firmware build | Notes |
| --- | --- | --- | --- |
| 2026-08-24 | Xteink X4 (ESP32-C3) | `main` at `e31a7142`, installed by SD-card update | Captain read a Chinese book with `LXGWWenKai` and `LXGWWenKaiTC` installed in `/.fonts/` and reported it works perfectly. See device verification notes below. |

## 2026-08-24 device verification (SD-card install)

The captain installed firmware build `main` at `e31a7142` on the Xteink X4.
The install method was an SD-card update, not USB flashing.
The USB-C path on this host still fails to enumerate, so SD-card update
stayed the only available install path for this pass.
Both `LXGWWenKai` and `LXGWWenKaiTC` families sat in `/.fonts/` for the test.

The captain read a Chinese book on the device and reported it works
perfectly.
This is a holistic pass, not a step-by-step walkthrough of every row in the
checklist above.
The captain did not report free heap or max alloc numbers at any point during
the test.

The checklist table above marks rows 1, 2, 4, 9, 10, 11, and 15 as a holistic
pass from this report, because normal reading exercises them.
Rows 6, 7, and 13 stay outside this pass.
The report does not cover the Text Anti-Aliasing toggle, the
anti-aliasing-off page-turn speed, or a Wi-Fi file-transfer round trip.
Steps 3, 5, 8, 12, and 14 stay pending, because the captain did not capture
heap figures.
The known unmeasured risk section above is still open.
The 4-style advance-cache aggregate and the transient merge peak still have
no device measurement.

## 2026-08-24 attempt (USB path)

The Xteink X4 did not enumerate over USB-C on the host for this verification
pass. The kernel logged: device descriptor read/64, error -71, port power
cycle, unable to enumerate.
`lsusb` and `pio device list` showed no ESP32-C3 serial device.
The checklist above was not run on hardware for this USB attempt.
The Result column stayed empty until the 2026-08-24 SD-card pass.

Before the next attempt, connect the USB-C cable again.
Try the opposite plug orientation.
Use a rear port on the host.
Confirm the device appears with `lsusb` and `pio device list`.
If the device still does not enumerate, hold the boot button during
connection to force download mode.

The simulator ran in place of hardware for these checks.
Read the results below for reference only.
The simulator cannot prove device heap behavior:

- `scripts/run_simulator_smoke_test.py --book test/epubs/test_chinese.epub
  --font-dir lib/EpdFont/scripts/output/cjk/LXGWWenKai --theme classic
  --page-turns 8`: passed.
- Same command with `--theme lyra-carousel`: passed.
- Same command against a real Simplified Chinese novel (137 chapters, long
  title, four authors in the OPF) with `--page-turns 20`: passed.
  The densest page carried 226 unique glyphs and a 38188-byte glyph bitmap.
  That count is close to the 224-glyph, 28.2 KB figure recorded in
  [Chinese Fonts](chinese-fonts.md#known-limits).
- The simulator's `ESP.getFreeHeap()` and `ESP.getMaxAllocHeap()` report a
  fixed 1 MB stub. None of the runs above produced a usable heap number.
  The known unmeasured risk above is still open.
