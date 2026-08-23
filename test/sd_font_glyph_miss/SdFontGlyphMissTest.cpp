#include <cstdio>

#include "lib/EpdFont/EpdFont.h"
#include "lib/EpdFont/EpdFontData.h"
#include "lib/EpdFont/EpdFontFamily.h"
#include "lib/Utf8/Utf8.h"

// ============================================================================
// Regression test for the chunked SD-font glyph arena render-miss bug.
//
// A real SdCardFont keeps only the current page's glyphs in its in-RAM
// interval table (the chunked bitmap arena). A glyph the arena had to drop
// is still reachable through EpdFontData::glyphMissHandler. That is a slow
// per-glyph SD read (SdCardFont::onGlyphMiss).
//
// Before this fix, EpdFontFamily::findGlyphData only called EpdFont::findGlyph.
// That checked the interval table only. It never consulted glyphMissHandler.
// A dropped glyph fell straight to the regular-style sibling, then to the
// replacement glyph. It drew as a box instead of the correct character.
//
// These tests use a fake glyphMissHandler that stands in for SdCardFont's
// overflow ring. It resolves one known "dropped" codepoint.
// It returns nullptr for everything else, the same as onGlyphMiss does on a
// genuine coverage miss.
// ============================================================================

static int testsPassed = 0;
static int testsFailed = 0;

#define ASSERT_TRUE(cond)                                                \
  do {                                                                   \
    if (!(cond)) {                                                       \
      fprintf(stderr, "  FAIL: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      testsFailed++;                                                     \
      return;                                                            \
    }                                                                    \
  } while (0)

#define ASSERT_EQ(a, b)                                                                                         \
  do {                                                                                                          \
    if ((a) != (b)) {                                                                                           \
      fprintf(stderr, "  FAIL: %s:%d: %s == %ld, expected %ld\n", __FILE__, __LINE__, #a, static_cast<long>(a), \
              static_cast<long>(b));                                                                            \
      testsFailed++;                                                                                            \
      return;                                                                                                   \
    }                                                                                                           \
  } while (0)

#define PASS() testsPassed++

// clang-format off

// Resident "arena" glyphs for the regular-style font: only 'A' and the
// replacement glyph fit. Everything else was dropped by the chunked arena.
static const EpdGlyph kRegularGlyphs[] = {
  // width  height  advanceX  left  top  dataLength  dataOffset
  /* 0 'A'              */ { 8,  8, 128, 0,  8, 0,   0 },
  /* 1 REPLACEMENT_GLYPH */ { 8,  8, 128, 0,  8, 0, 100 },
};

static const EpdUnicodeInterval kRegularIntervals[] = {
  { 0x41, 0x41, 0 },
  { REPLACEMENT_GLYPH, REPLACEMENT_GLYPH, 1 },
};

// Bold-style font: the resident arena holds only the replacement glyph.
// It has no glyphMissHandler of its own. This simulates a style whose SD
// section did not load, or one that already used up its own overflow ring.
static const EpdGlyph kBoldGlyphs[] = {
  /* 0 REPLACEMENT_GLYPH */ { 8, 8, 128, 0, 8, 0, 100 },
};

static const EpdUnicodeInterval kBoldIntervals[] = {
  { REPLACEMENT_GLYPH, REPLACEMENT_GLYPH, 0 },
};

// clang-format on

// Codepoint the chunked arena dropped, but that the SD overflow ring can
// still resolve on demand: U+4E2D (中).
constexpr uint32_t kOverflowCp = 0x4E2D;
static EpdGlyph kOverflowGlyph = {12, 12, 192, 0, 12, 0, 200};

// Codepoint genuinely absent from this font (neither arena nor overflow ring
// cover it), like a Latin character in a CJK-only font.
constexpr uint32_t kUncoveredCp = 0x4E00;

static int missHandlerCalls = 0;

static const EpdGlyph* fakeGlyphMissHandler(void* /*ctx*/, const uint32_t codepoint) {
  missHandlerCalls++;
  if (codepoint == kOverflowCp) return &kOverflowGlyph;
  return nullptr;
}

static const EpdFontData kRegularFontData = {
    .bitmap = nullptr,
    .glyph = kRegularGlyphs,
    .intervals = kRegularIntervals,
    .intervalCount = 2,
    .advanceY = 16,
    .ascender = 12,
    .descender = 0,
    .is2Bit = false,
    .groups = nullptr,
    .groupCount = 0,
    .glyphToGroup = nullptr,
    .kernLeftClasses = nullptr,
    .kernRightClasses = nullptr,
    .kernMatrix = nullptr,
    .kernLeftEntryCount = 0,
    .kernRightEntryCount = 0,
    .kernLeftClassCount = 0,
    .kernRightClassCount = 0,
    .ligaturePairs = nullptr,
    .ligaturePairCount = 0,
    .glyphMissHandler = fakeGlyphMissHandler,
    .glyphMissCtx = nullptr,
    .coverageHandler = nullptr,
};

static const EpdFontData kBoldFontData = {
    .bitmap = nullptr,
    .glyph = kBoldGlyphs,
    .intervals = kBoldIntervals,
    .intervalCount = 1,
    .advanceY = 16,
    .ascender = 12,
    .descender = 0,
    .is2Bit = false,
    .groups = nullptr,
    .groupCount = 0,
    .glyphToGroup = nullptr,
    .kernLeftClasses = nullptr,
    .kernRightClasses = nullptr,
    .kernMatrix = nullptr,
    .kernLeftEntryCount = 0,
    .kernRightEntryCount = 0,
    .kernLeftClassCount = 0,
    .kernRightClassCount = 0,
    .ligaturePairs = nullptr,
    .ligaturePairCount = 0,
    .glyphMissHandler = nullptr,
    .glyphMissCtx = nullptr,
    .coverageHandler = nullptr,
};

static EpdFont regularFont(&kRegularFontData);
static EpdFont boldFont(&kBoldFontData);
static EpdFontFamily fontFamily(&regularFont, &boldFont);

void testDroppedGlyphResolvesViaMissHandler() {
  printf("testDroppedGlyphResolvesViaMissHandler...\n");

  missHandlerCalls = 0;
  const EpdFontFamily::GlyphData glyphData = fontFamily.findGlyphData(kOverflowCp);

  ASSERT_TRUE(glyphData.glyph != nullptr);
  ASSERT_EQ(missHandlerCalls, 1);
  ASSERT_EQ(glyphData.glyph->advanceX, kOverflowGlyph.advanceX);

  printf("  Dropped glyph resolved through the SD overflow ring\n");
  PASS();
}

void testResidentGlyphSkipsMissHandler() {
  printf("testResidentGlyphSkipsMissHandler...\n");

  missHandlerCalls = 0;
  const EpdFontFamily::GlyphData glyphData = fontFamily.findGlyphData('A');

  ASSERT_TRUE(glyphData.glyph != nullptr);
  ASSERT_EQ(missHandlerCalls, 0);

  printf("  Resident glyph took the fast interval-table path only\n");
  PASS();
}

void testUncoveredCodepointStillMisses() {
  printf("testUncoveredCodepointStillMisses...\n");

  missHandlerCalls = 0;
  const EpdFontFamily::GlyphData glyphData = fontFamily.findGlyphData(kUncoveredCp);

  ASSERT_TRUE(glyphData.glyph == nullptr);
  ASSERT_EQ(missHandlerCalls, 1);

  printf("  A genuinely uncovered codepoint still reports a miss, not a false hit\n");
  PASS();
}

void testGetGlyphDataFallsBackToReplacementWhenUncovered() {
  printf("testGetGlyphDataFallsBackToReplacementWhenUncovered...\n");

  const EpdFontFamily::GlyphData glyphData = fontFamily.getGlyphData(kUncoveredCp);

  ASSERT_TRUE(glyphData.glyph != nullptr);
  ASSERT_EQ(glyphData.glyph->advanceX, kRegularGlyphs[1].advanceX);

  printf("  A truly uncovered codepoint still falls back to the replacement glyph\n");
  PASS();
}

void testBoldStyleFallsBackToRegularMissHandler() {
  printf("testBoldStyleFallsBackToRegularMissHandler...\n");

  missHandlerCalls = 0;
  const EpdFontFamily::GlyphData glyphData = fontFamily.findGlyphData(kOverflowCp, EpdFontFamily::BOLD);

  ASSERT_TRUE(glyphData.glyph != nullptr);
  ASSERT_EQ(missHandlerCalls, 1);
  ASSERT_EQ(glyphData.glyph->advanceX, kOverflowGlyph.advanceX);

  printf("  A bold style with no miss handler of its own reaches the regular sibling's overflow ring\n");
  PASS();
}

int main() {
  printf("=== SD Font Glyph Miss Tests ===\n\n");

  testDroppedGlyphResolvesViaMissHandler();
  testResidentGlyphSkipsMissHandler();
  testUncoveredCodepointStillMisses();
  testGetGlyphDataFallsBackToReplacementWhenUncovered();
  testBoldStyleFallsBackToRegularMissHandler();

  printf("\n=== Results: %d passed, %d failed ===\n", testsPassed, testsFailed);
  return testsFailed > 0 ? 1 : 0;
}
