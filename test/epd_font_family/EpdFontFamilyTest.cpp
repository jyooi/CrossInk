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

TEST_F(AsymmetricFamily, RegularStyleResolvesOnItsOwnFace) {
  const auto glyphData = family.getGlyphData(kCovered, EpdFontFamily::REGULAR);
  EXPECT_EQ(glyphData.fontData, &regularData);
  EXPECT_EQ(glyphData.glyph, &kRegularGlyphs[0]);
}
