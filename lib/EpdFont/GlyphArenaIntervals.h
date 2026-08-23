#pragma once

#include <cstdint>

#include "EpdFontData.h"

// Coalesces a codepoint-sorted, index-aligned list into EpdUnicodeInterval
// entries, including only the indices isPlaced() marks as resident. An
// index isPlaced() rejects breaks the current run, even when the codepoint
// that follows it is contiguous with the run before it.
//
// This backs the SD-card font glyph arena's degrade path (prewarmStyle).
// The chunked glyph-bitmap arena can run out of room partway through a
// page. The glyphs it placed still serve from the fast mini path.
// The rest are simply absent from these intervals.
// They fall through to the existing per-glyph overflow ring (onGlyphMiss).
// That ring loads them one at a time instead of losing the whole page.
//
// Mapping: any type with a `.codepoint` member, sorted ascending by it.
// mappings/count: the codepoint-sorted, index-aligned source list.
// isPlaced(i): true if index i should appear in the output intervals.
// outIntervals: caller-owned buffer of at least `count` entries. Each
// written interval's `offset` field is the index into `mappings` (and any
// other array sharing its index space, such as a glyph table) where that
// run starts.
// Returns the number of intervals written.
template <typename Mapping, typename IsPlaced>
uint32_t buildGlyphArenaIntervals(const Mapping* mappings, uint32_t count, IsPlaced isPlaced,
                                  EpdUnicodeInterval* outIntervals) {
  uint32_t intervalCount = 0;
  uint32_t rangeStart = UINT32_MAX;
  for (uint32_t i = 0; i < count; i++) {
    const bool placed = isPlaced(i);
    const bool breaksRun =
        rangeStart != UINT32_MAX && (!placed || mappings[i].codepoint != mappings[i - 1].codepoint + 1);
    if (breaksRun) {
      outIntervals[intervalCount].first = mappings[rangeStart].codepoint;
      outIntervals[intervalCount].last = mappings[i - 1].codepoint;
      outIntervals[intervalCount].offset = rangeStart;
      intervalCount++;
      rangeStart = UINT32_MAX;
    }
    if (placed && rangeStart == UINT32_MAX) rangeStart = i;
  }
  if (rangeStart != UINT32_MAX) {
    outIntervals[intervalCount].first = mappings[rangeStart].codepoint;
    outIntervals[intervalCount].last = mappings[count - 1].codepoint;
    outIntervals[intervalCount].offset = rangeStart;
    intervalCount++;
  }
  return intervalCount;
}
