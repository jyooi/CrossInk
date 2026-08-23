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
| 1 | Cold boot | Device boots to Home without a crash or reset loop | |
| 2 | Open the Chinese book from Home | Title renders in Chinese via the CJK UI fallback, no boxes | |
| 3 | Heap after open | Record free heap / max alloc | |
| 4 | 20 page turns, Text Anti-Aliasing on | Every page renders, no boxes, no crash, page-turn time feels consistent | |
| 5 | Heap after AA-on turns | Record free heap / max alloc | |
| 6 | Settings, Reader, Text Anti-Aliasing off | Setting takes effect immediately | |
| 7 | 20 page turns, Text Anti-Aliasing off | Every page renders, turns are faster than step 4 | |
| 8 | Heap after AA-off turns | Record free heap / max alloc | |
| 9 | Open Table of Contents | Chinese chapter titles render, wrap correctly between characters, no boxes | |
| 10 | Return to Home, check Recent Books | Chinese title renders correctly in the Recent Books row | |
| 11 | Sleep, then wake | Device wakes to the same page, no crash, no stale render | |
| 12 | Heap after sleep/wake | Record free heap / max alloc | |
| 13 | Start Wi-Fi file transfer, then exit | Web portal opens; after exit the CJK UI fallback still shows Chinese titles without a restart | |
| 14 | Heap after Wi-Fi transfer round trip | Record free heap / max alloc, compare against step 3 for a leak | |
| 15 | Dense chapter page (highest measured glyph count) | Page renders correctly; a glyph that misses the arena still draws through the slow per-glyph path instead of a box | |

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
| | | | |

## 2026-08-24 attempt

The Xteink X4 did not enumerate over USB-C on the host for this verification
pass. The kernel logged: device descriptor read/64, error -71, port power
cycle, unable to enumerate.
`lsusb` and `pio device list` showed no ESP32-C3 serial device.
The checklist above was not run on hardware.
The Result column is intentionally empty.

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
