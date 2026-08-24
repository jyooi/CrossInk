# CrossPoint Reader — Durable Context

Keep this file focused on repo-specific gotchas that are worth reusing in future sessions.

## FreeInk SDK

Refer to https://freeink.org/llms.txt for guidance.

## Simulator

- Simulator patches belong in the adjacent `crossink-simulator` repo.
- The valid local simulator env in this repo is `simulator`, and `pio run -e simulator` currently builds cleanly.
- The simulator `PNGdec` stub in `crossink-simulator/src/PNGdec.h` needs to mirror the real API shape used by app code, including `hasAlpha()` and `getTransparentColor()`, even though decode still fails intentionally.
- Known simulator limits:
  - No image rendering: `platformio.ini` ignores `hal`, `PNGdec`, and `JPEGDEC`, so image decoders are intentionally absent.
  - JPEGDEC stub always fails. `JPEGDEC fallback: open failed (err=-1)` is expected in simulator.
  - `esp_deep_sleep_start()` is a no-op in simulator.
  - `HalStorage` uses POSIX file access under `./fs_` and allows multiple readers, unlike real hardware.
- A Linux host with GCC 15 or later needs two extra `[simulator-base]` build flags.
  - `-std=gnu17`: GCC 15 sets plain C to C23 by default. C23 makes `bool`, `true`, and `false`
    keywords. The vendor QRCode library defines its own `bool` typedef, so this clash breaks it.
  - A Linux-only `-lcrypto` link flag: the vendor `MD5Builder_linux.h` calls OpenSSL `MD5_*`
    functions. The project had no matching link flag for this before.
  - A large `-Wnarrowing` error count in one generated array can also make GCC report an
    unrelated hard error later in the same file. Fix the real narrowing source first.
    Then check whether the second error still happens before you treat it as a separate bug.

## Real Hardware / Storage

- SdFat on hardware allows only one open reader per file path at a time. If a fallback needs to reopen the same file, close the first handle before reopening.

## Rendering / Reader Pipeline

- `lib/Epub/Epub/Page.cpp`: images must render only in `GfxRenderer::BW`; grayscale passes are text anti-aliasing passes only.
- Kindle EPUBs may contain paired high-res and old-Kindle fallback images. `ChapterHtmlSlimParser` should skip `<img>` nodes with `data-AmznRemoved-M8` to avoid duplicate stacked images.
- After image/layout pipeline changes that affect cached EPUB output, clear the affected `.crosspoint/epub_<hash>/` cache if behavior looks stale.

## UI Consistency

- Use FreeInkUI SDK components and input routing for list-style screens where possible. Row rendering, touch targets,
  hit testing, and pagination should share the same FreeInkUI list configuration instead of custom touch scaling.

## Heap Baselines (X4 hardware, SD card font)

- A normal resume-into-partial reading session runs at ~85-90KB free / ~49KB maxAlloc by
  the first watermark crossing (Epub metadata + x-locations + resident glyph caches).
  Do not read mid-range heap numbers as session degradation without checking the scenario.
- SD-font section builds cost ~38-50KB at cold start; the 4-style advance-table prewarm
  (~30KB incl. 16KB contiguous scratch) dominates and is skipped below 80KB free.
- CJK SD fonts use a 1024-entry advance cache (8 KB per style when full). Latin stays at 256.
  The cap is per style, not per font, and a page scan can request all 4 styles, so the
  worst-case resident cost is 4 x 8 KB = 32 KB of heap against 4 x 2 KB = 8 KB before.
  The `pio run -e default` RAM figure is static RAM and does not cover this.
  A merge also peaks at about 20 KB transient (8 KB scratch plus an 8 KB replacement
  table plus the 4 KB old table), needing two separate 8 KB contiguous blocks.
  Phase 4 still owes a device heap check for the 4-style aggregate and that peak.
  See `docs/chinese-fonts.md`.
- `prewarm()` reserves slot 0 of its codepoint buffer for U+FFFD. A page that needs more
  than 511 unique text glyphs still lists the replacement glyph. The codepoints the
  `MAX_PAGE_GLYPHS` cap drops render through the slow `onGlyphMiss` ring, like the
  glyphs the arena drops. This holds for the codepoint-list cap only.
  It does not promise that the U+FFFD bitmap reaches the chunked glyph arena.
- `SdCardFont`'s per-page glyph-bitmap arena (`lib/EpdFont/SdCardFont.cpp`) is chunked
  in fixed 4KB blocks (`MINI_BM_CHUNK_SIZE`), not one contiguous allocation.
  When a chunk fails, or the ceiling (`MINI_BM_MAX_CHUNKS` = 24) is hit,
  `prewarmStyle` keeps the placed glyphs and logs once per font, not per page.
  A dropped glyph now renders through `EpdFont::findGlyphOrMiss`, which asks the
  `onGlyphMiss` per-glyph SD overflow ring on an interval-table miss.
  See `docs/chinese-fonts.md` for overflow-ring capacity and per-miss SD cost.
  The desktop simulator's `ESP.getFreeHeap()/getMaxAllocHeap()` are a fixed 1MB stub.
  Reproducing arena degrade needs the `CROSSINK_SIM_ARENA_CHUNK_LIMIT` debug knob there,
  compiled only under `#ifdef SIMULATOR` (see `test/README`).

## Misc Repo Gotchas

- POSIX TZ signs are inverted from ISO 8601 in `TimeStore::applyTimezone()`: `"UTC-1"` means UTC+1.
- `LyraTheme::drawHeader()` does not call `BaseTheme::drawHeader()`, so header changes in the base theme must be duplicated in Lyra if needed.
