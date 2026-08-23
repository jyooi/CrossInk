#include <gtest/gtest.h>

#include <algorithm>
#include <vector>

#include "AdvanceTableMerge.h"
#include "SdCardFont.h"

namespace {

struct TestEntry {
  uint32_t codepoint;
  uint16_t advanceX;
};

std::vector<TestEntry> run(uint32_t first, uint32_t count, uint16_t advance) {
  std::vector<TestEntry> out;
  out.reserve(count);
  for (uint32_t i = 0; i < count; i++) {
    out.push_back({first + i, advance});
  }
  return out;
}

std::vector<uint32_t> codepointsOf(const std::vector<TestEntry>& entries, uint32_t count) {
  std::vector<uint32_t> out;
  out.reserve(count);
  for (uint32_t i = 0; i < count; i++) out.push_back(entries[i].codepoint);
  return out;
}

}  // namespace

TEST(AdvanceCacheLimit, CjkFontsGetMoreCapacityThanLatinFonts) {
  const uint32_t latin = SdCardFont::advanceCacheLimitFor(false);
  const uint32_t cjk = SdCardFont::advanceCacheLimitFor(true);
  EXPECT_GT(cjk, latin);
  // A Hongloumeng section asked for 890 unique codepoints and thrashed a
  // 256-entry cache. The CJK limit has to clear that in one table.
  EXPECT_GE(cjk, 890u);
  EXPECT_LT(latin, 890u);
}

TEST(AdvanceTableMerge, InterleavesTwoSortedRunsInCodepointOrder) {
  const std::vector<TestEntry> existing{{10, 1}, {30, 3}, {50, 5}};
  const std::vector<TestEntry> incoming{{20, 2}, {40, 4}};
  std::vector<TestEntry> out(8);

  const uint32_t written =
      mergeSortedAdvanceEntries(existing.data(), existing.size(), incoming.data(), incoming.size(), out.data(), 8);

  ASSERT_EQ(written, 5u);
  const std::vector<uint32_t> expected{10, 20, 30, 40, 50};
  EXPECT_EQ(codepointsOf(out, written), expected);
  EXPECT_EQ(out[1].advanceX, 2);
  EXPECT_EQ(out[4].advanceX, 5);
}

TEST(AdvanceTableMerge, DropsTheHighestCodepointsWhenTheCapIsReached) {
  const auto existing = run(1000, 600, 7);
  const auto incoming = run(2000, 600, 9);
  const uint32_t cap = SdCardFont::advanceCacheLimitFor(true);
  std::vector<TestEntry> out(cap);

  const uint32_t written =
      mergeSortedAdvanceEntries(existing.data(), existing.size(), incoming.data(), incoming.size(), out.data(), cap);

  ASSERT_EQ(written, cap);
  EXPECT_EQ(out[0].codepoint, 1000u);
  // 600 old entries, then the lowest of the incoming run. The tail drops.
  EXPECT_EQ(out[599].codepoint, 1599u);
  EXPECT_EQ(out[600].codepoint, 2000u);
  EXPECT_EQ(out[cap - 1].codepoint, 2000u + (cap - 600) - 1);
}

// The next three tests guard the grow-failure fallback. mergeIntoAdvanceTable
// keeps the capacity it already owns when an 8 KB grow fails on a fragmented C3
// heap. Before that fallback it dropped the whole merge, so the cache froze and
// layout reopened the font file for every character.
TEST(AdvanceTableMerge, AbandonsTheMergeWhenNoNewEntryWouldFit) {
  const auto existing = run(4000, 512, 7);
  const auto incoming = run(5000, 200, 9);
  const uint32_t existingCapacity = 512;
  std::vector<TestEntry> out(existing.size() + incoming.size());

  const uint32_t full = mergeSortedAdvanceEntries(existing.data(), existing.size(), incoming.data(), incoming.size(),
                                                  out.data(), out.size());
  ASSERT_EQ(full, 712u);

  const uint32_t fits = advanceMergeRetainCount(full, existingCapacity, existing.size());
  ASSERT_EQ(fits, 0u);
}

TEST(AdvanceTableMerge, AShortWriteStillKeepsEveryExistingEntry) {
  const auto existing = run(4000, 400, 7);
  const auto incoming = run(5000, 200, 9);
  const uint32_t existingCapacity = 512;
  std::vector<TestEntry> out(existing.size() + incoming.size());

  const uint32_t full = mergeSortedAdvanceEntries(existing.data(), existing.size(), incoming.data(), incoming.size(),
                                                  out.data(), out.size());
  ASSERT_EQ(full, 600u);

  const uint32_t fits = advanceMergeRetainCount(full, existingCapacity, existing.size());
  ASSERT_EQ(fits, existingCapacity);
  const std::vector<uint32_t> retained = codepointsOf(out, fits);
  ASSERT_GT(retained.size(), existing.size());
  EXPECT_TRUE(std::equal(existing.begin(), existing.end(), retained.begin(),
                         [](const TestEntry& e, uint32_t cp) { return e.codepoint == cp; }));
  EXPECT_EQ(retained.back(), 5111u);
}

TEST(AdvanceTableMerge, AdmitsNewEntriesWhenSpareCapacityRemains) {
  const auto existing = run(4000, 300, 7);
  const auto incoming = run(5000, 200, 9);
  const uint32_t existingCapacity = 512;
  std::vector<TestEntry> out(existing.size() + incoming.size());

  const uint32_t full = mergeSortedAdvanceEntries(existing.data(), existing.size(), incoming.data(), incoming.size(),
                                                  out.data(), out.size());
  ASSERT_EQ(full, 500u);

  const uint32_t fits = advanceMergeRetainCount(full, existingCapacity, existing.size());
  EXPECT_EQ(fits, 500u);
  EXPECT_GT(fits, existing.size());
  EXPECT_EQ(out[fits - 1].codepoint, 5199u);
}
