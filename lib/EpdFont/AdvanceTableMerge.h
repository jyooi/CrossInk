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
// The truncation drops by codepoint, not by source run, so a short write can
// drop entries that came from `a`. A caller that must keep every entry of `a`
// has to call mergeRetainingAllExisting() below instead.
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

// Merges into `outCap` entries while keeping every entry of `a`. Only `b` is
// truncated, and it is truncated from its highest codepoints, so the entries
// admitted are the lowest new codepoints and the output stays sorted.
//
// This is the fallback for a caller that cannot grow its table. `outCap` is the
// capacity it already owns. Returns the number written, which is `aCount` when
// no new entry fits, and 0 when `outCap` cannot even hold `a`. A return value
// of `aCount` or less means the caller should keep the table it has.
template <typename Entry>
uint32_t mergeRetainingAllExisting(const Entry* a, const uint32_t aCount, const Entry* b, const uint32_t bCount,
                                   Entry* out, const uint32_t outCap) {
  if (outCap < aCount) return 0;
  uint32_t admitted = outCap - aCount;
  if (admitted > bCount) admitted = bCount;
  return mergeSortedAdvanceEntries(a, aCount, b, admitted, out, aCount + admitted);
}
