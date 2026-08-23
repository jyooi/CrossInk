#pragma once

#include <cstdint>

// Merges two codepoint-sorted, non-overlapping advance runs into `out`,
// writing at most `outCap` entries and returning how many it wrote.
//
// This backs the SD-card font advance cache (SdCardFont::mergeIntoAdvanceTable).
// The cache is capped per font: 256 entries for Latin, 1024 for CJK. A merge
// that would exceed the cap drops entries from the tail, which is the highest
// codepoints. The drop is deterministic and the output stays sorted.
//
// The truncation also makes a short write safe. For any `outCap` at least as
// large as `aCount`, the prefix `out[0..returned)` still holds every entry
// from `a`. The caller can therefore fall back to the storage it already owns
// when it cannot grow the table, instead of discarding the whole merge.
//
// Entry: any type with a `.codepoint` member, comparable with <=.
// a/aCount: the existing run, sorted ascending by codepoint.
// b/bCount: the incoming run, sorted ascending by codepoint, disjoint from `a`.
// out/outCap: caller-owned destination buffer and its entry capacity.
template <typename Entry>
uint32_t mergeSortedAdvanceEntries(const Entry* a, const uint32_t aCount, const Entry* b, const uint32_t bCount,
                                   Entry* out, const uint32_t outCap) {
  uint32_t i = 0, j = 0, k = 0;
  while (k < outCap && (i < aCount || j < bCount)) {
    if (i < aCount && (j >= bCount || a[i].codepoint <= b[j].codepoint)) {
      out[k++] = a[i++];
    } else {
      out[k++] = b[j++];
    }
  }
  return k;
}

// Chooses how many merged entries to write back when the table cannot grow to
// hold all `merged` of them. `capacity` is the storage the caller already owns
// and `oldCount` is what that storage held before the merge.
//
// Returns 0 when no new entry would fit, which tells the caller to abandon the
// merge and keep the table it has. Otherwise it returns a count above
// `oldCount`, so the write admits the lowest new codepoints and never loses a
// cached advance. Never returns a value below `oldCount`.
constexpr uint32_t advanceMergeRetainCount(const uint32_t merged, const uint32_t capacity, const uint32_t oldCount) {
  const uint32_t fits = capacity < merged ? capacity : merged;
  return fits > oldCount ? fits : 0;
}
