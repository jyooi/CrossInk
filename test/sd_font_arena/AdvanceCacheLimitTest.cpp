#include <gtest/gtest.h>

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

// The next four tests guard the grow-failure fallback. mergeIntoAdvanceTable
// re-merges into the capacity it already owns when an 8 KB grow fails on a
// fragmented C3 heap. It must keep every cached entry and admit only the lowest
// new codepoints that still fit, whatever the two runs' relative order is.
TEST(AdvanceTableMergeFallback, KeepsTheTableWhenNoNewEntryFits) {
  const auto existing = run(4000, 512, 7);
  const auto incoming = run(5000, 200, 9);
  std::vector<TestEntry> out(existing.size() + incoming.size());

  const uint32_t written =
      mergeRetainingAllExisting(existing.data(), existing.size(), incoming.data(), incoming.size(), out.data(), 512);

  EXPECT_EQ(written, existing.size());
}

TEST(AdvanceTableMergeFallback, AdmitsTheLowestNewCodepointsThatFit) {
  const auto existing = run(4000, 400, 7);
  const auto incoming = run(5000, 200, 9);
  std::vector<TestEntry> out(existing.size() + incoming.size());

  const uint32_t written =
      mergeRetainingAllExisting(existing.data(), existing.size(), incoming.data(), incoming.size(), out.data(), 512);

  ASSERT_EQ(written, 512u);
  EXPECT_EQ(codepointsOf(out, 400), codepointsOf(existing, existing.size()));
  EXPECT_EQ(out[400].codepoint, 5000u);
  EXPECT_EQ(out[511].codepoint, 5111u);
}

// The reversed case: every incoming codepoint sorts below every cached one, so
// a plain prefix truncation would drop the 88 highest cached entries.
TEST(AdvanceTableMergeFallback, KeepsCachedEntriesThatSortAboveTheNewOnes) {
  const auto existing = run(9000, 400, 7);
  const auto incoming = run(4000, 200, 9);
  std::vector<TestEntry> out(existing.size() + incoming.size());

  const uint32_t written =
      mergeRetainingAllExisting(existing.data(), existing.size(), incoming.data(), incoming.size(), out.data(), 512);

  ASSERT_EQ(written, 512u);
  // 112 admitted new entries first, then all 400 cached entries, still sorted.
  EXPECT_EQ(out[0].codepoint, 4000u);
  EXPECT_EQ(out[111].codepoint, 4111u);
  const std::vector<uint32_t> merged = codepointsOf(out, written);
  const std::vector<uint32_t> tail(merged.begin() + 112, merged.end());
  EXPECT_EQ(tail, codepointsOf(existing, existing.size()));
  EXPECT_EQ(out[written - 1].codepoint, 9399u);
}

TEST(AdvanceTableMergeFallback, ReportsFailureWhenTheCapacityCannotHoldTheCache) {
  const auto existing = run(4000, 400, 7);
  const auto incoming = run(5000, 200, 9);
  std::vector<TestEntry> out(existing.size() + incoming.size());

  const uint32_t written =
      mergeRetainingAllExisting(existing.data(), existing.size(), incoming.data(), incoming.size(), out.data(), 300);

  EXPECT_EQ(written, 0u);
}
