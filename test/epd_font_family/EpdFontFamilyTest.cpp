#include <Utf8.h>
#include <gtest/gtest.h>

#include "EpdFontFamily.h"

namespace {

// Two faces with deliberately asymmetric coverage, mirroring an SD family whose
// bold face carries a narrower fallback chain than its regular face.
// Regular covers 'A' and U+FFFD. Bold covers only U+FFFD.

constexpr uint32_t kCovered = 'A';

const EpdGlyph kRegularGlyphs[] = {
    {4, 6, 4 << 4, 0, 6, 6, 0},  // 'A'
    {5, 7, 5 << 4, 0, 7, 7, 6},  // U+FFFD
};
const EpdUnicodeInterval kRegularIntervals[] = {
    {kCovered, kCovered, 0},
    {REPLACEMENT_GLYPH, REPLACEMENT_GLYPH, 1},
};
const uint8_t kRegularBitmap[13] = {};

const EpdGlyph kBoldGlyphs[] = {
    {5, 7, 5 << 4, 0, 7, 7, 0},  // U+FFFD only
};
const EpdUnicodeInterval kBoldIntervals[] = {
    {REPLACEMENT_GLYPH, REPLACEMENT_GLYPH, 0},
};
const uint8_t kBoldBitmap[7] = {};

EpdFontData makeFontData(const EpdGlyph* glyphs, const EpdUnicodeInterval* intervals, uint32_t intervalCount,
                         const uint8_t* bitmap) {
  EpdFontData data = {};
  data.bitmap = bitmap;
  data.glyph = glyphs;
  data.intervals = intervals;
  data.intervalCount = intervalCount;
  data.advanceY = 10;
  data.ascender = 8;
  data.descender = -2;
  return data;
}

class AsymmetricFamily : public ::testing::Test {
 protected:
  AsymmetricFamily()
      : regularData(makeFontData(kRegularGlyphs, kRegularIntervals, 2, kRegularBitmap)),
        boldData(makeFontData(kBoldGlyphs, kBoldIntervals, 1, kBoldBitmap)),
        regular(&regularData),
        bold(&boldData),
        family(&regular, &bold) {}

  EpdFontData regularData;
  EpdFontData boldData;
  EpdFont regular;
  EpdFont bold;
  EpdFontFamily family;
};

}  // namespace

TEST_F(AsymmetricFamily, StyledFaceFallsBackToRegularRealGlyph) {
  const EpdGlyph* glyph = family.getGlyph(kCovered, EpdFontFamily::BOLD);
  ASSERT_NE(glyph, nullptr);
  EXPECT_EQ(glyph, &kRegularGlyphs[0]);
  EXPECT_NE(glyph, &kBoldGlyphs[0]);
}

TEST_F(AsymmetricFamily, LoadGlyphDataReturnsTheFaceThatSuppliedTheGlyph) {
  const auto glyphData = family.getGlyphData(kCovered, EpdFontFamily::BOLD);
  ASSERT_NE(glyphData.glyph, nullptr);
  ASSERT_NE(glyphData.fontData, nullptr);
  EXPECT_EQ(glyphData.fontData, &regularData);

  // The bitmap paths index fontData->glyph and fontData->bitmap with the
  // returned glyph. Pairing it with the styled face would index out of range.
  const ptrdiff_t glyphIndex = glyphData.glyph - glyphData.fontData->glyph;
  EXPECT_GE(glyphIndex, 0);
  EXPECT_LT(glyphIndex, 2);
  EXPECT_LT(glyphData.glyph->dataOffset + glyphData.glyph->dataLength, sizeof(kRegularBitmap) + 1);
}

TEST_F(AsymmetricFamily, UncoveredCodepointStillFallsBackToTheReplacementGlyph) {
  const auto glyphData = family.getGlyphData(0x4E2D, EpdFontFamily::BOLD);
  ASSERT_NE(glyphData.glyph, nullptr);
  EXPECT_EQ(glyphData.fontData, &boldData);
  EXPECT_EQ(glyphData.glyph, &kBoldGlyphs[0]);
}

TEST_F(AsymmetricFamily, FallbackCodepointAcceptsCoverageOnTheRegularFace) {
  // getFallbackCodepoint decides whether the caller draws the real character or
  // a U+FFFD box. Probing only the styled face reports a miss for a glyph
  // getGlyphData would have found on regular.
  EXPECT_EQ(family.getFallbackCodepoint(kCovered, EpdFontFamily::BOLD), kCovered);
  EXPECT_TRUE(family.hasCodepointInFamily(kCovered, EpdFontFamily::BOLD));
  EXPECT_FALSE(family.hasCodepoint(kCovered, EpdFontFamily::BOLD));
}

TEST_F(AsymmetricFamily, FallbackCodepointStillBoxesWhatNoFaceCovers) {
  EXPECT_EQ(family.getFallbackCodepoint(0x4E2D, EpdFontFamily::BOLD), REPLACEMENT_GLYPH);
  EXPECT_FALSE(family.hasCodepointInFamily(0x4E2D, EpdFontFamily::BOLD));
}

TEST_F(AsymmetricFamily, RegularStyleResolvesOnItsOwnFace) {
  const auto glyphData = family.getGlyphData(kCovered, EpdFontFamily::REGULAR);
  EXPECT_EQ(glyphData.fontData, &regularData);
  EXPECT_EQ(glyphData.glyph, &kRegularGlyphs[0]);
}

namespace {

// Models an SD card font: the interval table covers only the current page, and
// anything else is fetched on demand into a glyph that lives outside that table.
// SdCardFont::onGlyphMiss returns exactly such a pointer.

constexpr uint32_t kOnDemandCp = 0x4E2D;  // 中

const EpdGlyph kOnDemandGlyph = {9, 9, 9 << 4, 0, 9, 12, 0};

struct MissCounter {
  int calls = 0;
};

const EpdGlyph* onGlyphMiss(void* ctx, const uint32_t codepoint) {
  if (codepoint != kOnDemandCp) return nullptr;
  static_cast<MissCounter*>(ctx)->calls++;
  return &kOnDemandGlyph;
}

class PagedFace : public ::testing::Test {
 protected:
  PagedFace()
      : pageData(makeFontData(kRegularGlyphs, kRegularIntervals, 2, kRegularBitmap)), page(nullptr), family(nullptr) {
    pageData.glyphMissHandler = onGlyphMiss;
    pageData.glyphMissCtx = &counter;
    page = EpdFont(&pageData);
    family = EpdFontFamily(&page);
  }

  MissCounter counter;
  EpdFontData pageData;
  EpdFont page;
  EpdFontFamily family;
};

}  // namespace

TEST_F(PagedFace, GetGlyphDataResolvesGlyphsOutsideTheIntervalTable) {
  const auto glyphData = family.getGlyphData(kOnDemandCp);
  ASSERT_NE(glyphData.glyph, nullptr);
  EXPECT_EQ(glyphData.glyph, &kOnDemandGlyph);
  EXPECT_EQ(glyphData.fontData, &pageData);
  EXPECT_EQ(counter.calls, 1);
}

TEST_F(PagedFace, PixelResolverAgreesWithTheMetricsResolver) {
  // drawText takes the cursor advance from getGlyph(); the pixel path must draw
  // that same glyph. Resolving pixels through the interval table alone paints a
  // U+FFFD box at the real character's advance.
  const EpdGlyph* metricsGlyph = family.getGlyph(kOnDemandCp);
  const EpdGlyph* pixelGlyph = family.getGlyphData(kOnDemandCp).glyph;
  ASSERT_NE(metricsGlyph, nullptr);
  EXPECT_EQ(pixelGlyph, metricsGlyph);
  EXPECT_EQ(pixelGlyph->advanceX, kOnDemandGlyph.advanceX);
}

TEST_F(PagedFace, CodepointTheHandlerRejectsStillFallsBackToTheReplacementGlyph) {
  const auto glyphData = family.getGlyphData(0x4E00);
  ASSERT_NE(glyphData.glyph, nullptr);
  EXPECT_EQ(glyphData.glyph, &kRegularGlyphs[1]);
  EXPECT_EQ(glyphData.fontData, &pageData);
}
