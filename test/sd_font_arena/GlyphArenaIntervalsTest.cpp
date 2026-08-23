#include "GlyphArenaIntervals.h"

#include <gtest/gtest.h>

#include <vector>

namespace {

struct TestMapping {
  uint32_t codepoint;
};

std::vector<TestMapping> makeMappings(const std::vector<uint32_t>& codepoints) {
  std::vector<TestMapping> out;
  out.reserve(codepoints.size());
  for (uint32_t cp : codepoints) out.push_back({cp});
  return out;
}

// isPlaced() that always returns true, matching a metadata-only prewarm
// where every requested codepoint is resident.
auto allPlaced = [](uint32_t) { return true; };

}  // namespace

TEST(GlyphArenaIntervals, EmptyInputProducesNoIntervals) {
  std::vector<EpdUnicodeInterval> out(1);
  const uint32_t count = buildGlyphArenaIntervals<TestMapping>(nullptr, 0, allPlaced, out.data());
  EXPECT_EQ(count, 0u);
}

TEST(GlyphArenaIntervals, SingleGlyphProducesOneInterval) {
  auto mappings = makeMappings({42});
  std::vector<EpdUnicodeInterval> out(1);
  const uint32_t count = buildGlyphArenaIntervals(mappings.data(), mappings.size(), allPlaced, out.data());
  ASSERT_EQ(count, 1u);
  EXPECT_EQ(out[0].first, 42u);
  EXPECT_EQ(out[0].last, 42u);
  EXPECT_EQ(out[0].offset, 0u);
}

TEST(GlyphArenaIntervals, FullyResidentContiguousRunCoalescesToOneInterval) {
  auto mappings = makeMappings({10, 11, 12, 13, 14});
  std::vector<EpdUnicodeInterval> out(mappings.size());
  const uint32_t count = buildGlyphArenaIntervals(mappings.data(), mappings.size(), allPlaced, out.data());
  ASSERT_EQ(count, 1u);
  EXPECT_EQ(out[0].first, 10u);
  EXPECT_EQ(out[0].last, 14u);
  EXPECT_EQ(out[0].offset, 0u);
}

TEST(GlyphArenaIntervals, CodepointGapSplitsIntervalsWhenFullyResident) {
  auto mappings = makeMappings({10, 11, 12, 20, 21});
  std::vector<EpdUnicodeInterval> out(mappings.size());
  const uint32_t count = buildGlyphArenaIntervals(mappings.data(), mappings.size(), allPlaced, out.data());
  ASSERT_EQ(count, 2u);
  EXPECT_EQ(out[0].first, 10u);
  EXPECT_EQ(out[0].last, 12u);
  EXPECT_EQ(out[0].offset, 0u);
  EXPECT_EQ(out[1].first, 20u);
  EXPECT_EQ(out[1].last, 21u);
  EXPECT_EQ(out[1].offset, 3u);
}

// This is the degrade path's core contract: a glyph the arena could not
// place breaks the run even though the codepoint sequence has no gap.
TEST(GlyphArenaIntervals, UnplacedIndexInMiddleOfRunSplitsAndExcludesIt) {
  auto mappings = makeMappings({10, 11, 12, 13, 14});
  auto placed = [](uint32_t i) { return i != 2; };  // codepoint 12 (index 2) missed the arena
  std::vector<EpdUnicodeInterval> out(mappings.size());
  const uint32_t count = buildGlyphArenaIntervals(mappings.data(), mappings.size(), placed, out.data());
  ASSERT_EQ(count, 2u);
  EXPECT_EQ(out[0].first, 10u);
  EXPECT_EQ(out[0].last, 11u);
  EXPECT_EQ(out[0].offset, 0u);
  EXPECT_EQ(out[1].first, 13u);
  EXPECT_EQ(out[1].last, 14u);
  EXPECT_EQ(out[1].offset, 3u);
}

TEST(GlyphArenaIntervals, UnplacedLeadingIndexIsExcluded) {
  auto mappings = makeMappings({10, 11, 12});
  auto placed = [](uint32_t i) { return i != 0; };
  std::vector<EpdUnicodeInterval> out(mappings.size());
  const uint32_t count = buildGlyphArenaIntervals(mappings.data(), mappings.size(), placed, out.data());
  ASSERT_EQ(count, 1u);
  EXPECT_EQ(out[0].first, 11u);
  EXPECT_EQ(out[0].last, 12u);
  EXPECT_EQ(out[0].offset, 1u);
}

TEST(GlyphArenaIntervals, UnplacedTrailingIndexIsExcluded) {
  auto mappings = makeMappings({10, 11, 12});
  auto placed = [](uint32_t i) { return i != 2; };
  std::vector<EpdUnicodeInterval> out(mappings.size());
  const uint32_t count = buildGlyphArenaIntervals(mappings.data(), mappings.size(), placed, out.data());
  ASSERT_EQ(count, 1u);
  EXPECT_EQ(out[0].first, 10u);
  EXPECT_EQ(out[0].last, 11u);
  EXPECT_EQ(out[0].offset, 0u);
}

// Models the arena filling up partway through a dense page. Everything from
// a cutoff index onward missed the chunked arena. It must fall through to
// the per-glyph overflow ring.
TEST(GlyphArenaIntervals, TailCutoffLikeArenaExhaustionKeepsOnlyThePrefix) {
  auto mappings = makeMappings({100, 101, 102, 103, 104, 105});
  const uint32_t cutoff = 4;
  auto placed = [cutoff](uint32_t i) { return i < cutoff; };
  std::vector<EpdUnicodeInterval> out(mappings.size());
  const uint32_t count = buildGlyphArenaIntervals(mappings.data(), mappings.size(), placed, out.data());
  ASSERT_EQ(count, 1u);
  EXPECT_EQ(out[0].first, 100u);
  EXPECT_EQ(out[0].last, 103u);
  EXPECT_EQ(out[0].offset, 0u);
}

TEST(GlyphArenaIntervals, AllUnplacedProducesNoIntervals) {
  auto mappings = makeMappings({10, 11, 12});
  auto placed = [](uint32_t) { return false; };
  std::vector<EpdUnicodeInterval> out(mappings.size());
  const uint32_t count = buildGlyphArenaIntervals(mappings.data(), mappings.size(), placed, out.data());
  EXPECT_EQ(count, 0u);
}

// Non-adjacent placement, as a fragmented heap can leave arbitrary chunk
// allocations to succeed while others fail mid-page.
TEST(GlyphArenaIntervals, ScatteredPlacementProducesOneIntervalPerPlacedGlyph) {
  auto mappings = makeMappings({10, 11, 12, 13, 14});
  auto placed = [](uint32_t i) { return i == 0 || i == 2 || i == 4; };
  std::vector<EpdUnicodeInterval> out(mappings.size());
  const uint32_t count = buildGlyphArenaIntervals(mappings.data(), mappings.size(), placed, out.data());
  ASSERT_EQ(count, 3u);
  EXPECT_EQ(out[0].first, 10u);
  EXPECT_EQ(out[0].last, 10u);
  EXPECT_EQ(out[0].offset, 0u);
  EXPECT_EQ(out[1].first, 12u);
  EXPECT_EQ(out[1].last, 12u);
  EXPECT_EQ(out[1].offset, 2u);
  EXPECT_EQ(out[2].first, 14u);
  EXPECT_EQ(out[2].last, 14u);
  EXPECT_EQ(out[2].offset, 4u);
}
