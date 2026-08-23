#include <gtest/gtest.h>

#include <set>
#include <string>
#include <vector>

#include "Hyphenator.h"
#include "Utf8.h"

// Byte sequences below use hex escapes, matching Utf8ComposeTest, so this file does
// not depend on the source encoding used to store it.
namespace {

const std::string kHan1 = "\xE4\xB8\x80";                 // U+4E00
const std::string kHan2 = "\xE4\xBA\x8C";                 // U+4E8C
const std::string kHan3 = "\xE4\xB8\x89";                 // U+4E09
const std::string kHan4 = "\xE5\x9B\x9B";                 // U+56DB
const std::string kHan5 = "\xE4\xBA\x94";                 // U+4E94
const std::string kHan6 = "\xE5\x85\xAD";                 // U+516D
const std::string kHan7 = "\xE4\xB8\x83";                 // U+4E03
const std::string kHan8 = "\xE5\x85\xAB";                 // U+516B
const std::string kHan9 = "\xE4\xB9\x9D";                 // U+4E5D
const std::string kNi = "\xE4\xBD\xA0";                   // U+4F60
const std::string kHao = "\xE5\xA5\xBD";                  // U+597D
const std::string kIdeographicComma = "\xE3\x80\x81";     // U+3001, no-line-start
const std::string kEllipsis = "\xE2\x80\xA6";             // U+2026
const std::string kEmDash = "\xE2\x80\x94";               // U+2014
const std::string kReversedQuote = "\xE3\x80\x9D";        // U+301D, no-line-end
const std::string kVariationSelector16 = "\xEF\xB8\x8F";  // U+FE0F

std::vector<uint32_t> toCodepoints(const std::string& text) {
  std::vector<uint32_t> out;
  const auto* ptr = reinterpret_cast<const unsigned char*>(text.c_str());
  while (*ptr) {
    out.push_back(utf8NextCodepoint(&ptr));
  }
  return out;
}

}  // namespace

// AGENTS.md section 2 records this as already present in the tree.
TEST(CjkBreakOpportunity, BreaksBetweenHanCharacters) {
  const auto cps = toCodepoints(kNi + kHao);
  ASSERT_EQ(cps.size(), 2u);
  EXPECT_TRUE(utf8HasCjkBreakOpportunityBetween(cps[0], cps[1]));

  const auto offsets = utf8CjkCharacterBreakByteOffsets(kNi + kHao);
  EXPECT_EQ(offsets, (std::vector<size_t>{3}));
}

TEST(CjkBreakOpportunity, NoLineStartMarksCannotOpenALine) {
  // clang-format off
  const std::vector<std::string> marks = {
      kEllipsis,          // U+2026
      kEmDash,            // U+2014
      "\xEF\xBD\x9E",     // U+FF5E fullwidth tilde
      "\xC2\xB7",         // U+00B7 middle dot
      "\xE3\x83\xBB",     // U+30FB katakana middle dot
      "\xE3\x80\x85",     // U+3005 ideographic iteration mark
      "\xE3\x83\xBC",     // U+30FC prolonged sound mark
      kIdeographicComma,  // U+3001
      "\xE3\x80\x82",     // U+3002 pre-existing entry
      "\xE3\x80\x9E",     // U+301E double prime quotation mark
      "\xE3\x80\x9F",     // U+301F low double prime quotation mark
      "\xEF\xBD\xA1",     // U+FF61 halfwidth ideographic full stop
      "\xEF\xBD\xA3",     // U+FF63 halfwidth right corner bracket
      "\xEF\xBD\xA4",     // U+FF64 halfwidth ideographic comma
      "\xE3\x81\x81",     // U+3041 small hiragana a
      "\xE3\x82\xA1",     // U+30A1 small katakana a
  };
  // clang-format on
  for (const auto& mark : marks) {
    const auto cps = toCodepoints(kHao + mark);
    ASSERT_EQ(cps.size(), 2u) << mark;
    EXPECT_TRUE(utf8IsNoLineStartMark(cps[1])) << mark;
    EXPECT_FALSE(utf8HasCjkBreakOpportunityBetween(cps[0], cps[1])) << mark;
  }
}

TEST(CjkBreakOpportunity, NoLineEndMarksCannotCloseALine) {
  const std::vector<std::string> marks = {
      kReversedQuote,  // U+301D
      "\xEF\xBD\xA2",  // U+FF62 halfwidth left corner bracket
  };
  for (const auto& mark : marks) {
    const auto cps = toCodepoints(mark + kHao);
    ASSERT_EQ(cps.size(), 2u) << mark;
    EXPECT_TRUE(utf8IsNoLineEndMark(cps[0])) << mark;
    EXPECT_FALSE(utf8HasCjkBreakOpportunityBetween(cps[0], cps[1])) << mark;
  }
}

TEST(CjkBreakOpportunity, DashAndEllipsisPairsDoNotSplit) {
  // Both marks are no-line-start only: neither may open a line, so a break may not sit
  // just before the first mark (it would strand the first mark at the top of a new
  // line) or between the two marks (it would strand the second mark there instead).
  // A break right after the second mark is fine.
  const auto ellipsisPair = toCodepoints(kNi + kEllipsis + kEllipsis + kHao);
  ASSERT_EQ(ellipsisPair.size(), 4u);
  EXPECT_FALSE(utf8HasCjkBreakOpportunityBetween(ellipsisPair[0], ellipsisPair[1]));
  EXPECT_FALSE(utf8HasCjkBreakOpportunityBetween(ellipsisPair[1], ellipsisPair[2]));
  EXPECT_TRUE(utf8HasCjkBreakOpportunityBetween(ellipsisPair[2], ellipsisPair[3]));

  const auto dashPair = toCodepoints(kNi + kEmDash + kEmDash + kHao);
  ASSERT_EQ(dashPair.size(), 4u);
  EXPECT_FALSE(utf8HasCjkBreakOpportunityBetween(dashPair[0], dashPair[1]));
  EXPECT_FALSE(utf8HasCjkBreakOpportunityBetween(dashPair[1], dashPair[2]));
  EXPECT_TRUE(utf8HasCjkBreakOpportunityBetween(dashPair[2], dashPair[3]));

  // ni(3) emdash(3) emdash(3) hao(3) -- only the offset after the second dash survives.
  const auto offsets = utf8CjkCharacterBreakByteOffsets(kNi + kEmDash + kEmDash + kHao);
  EXPECT_EQ(offsets, (std::vector<size_t>{9}));
}

TEST(CjkBreakOpportunity, VariationSelectorStaysWithBase) {
  const auto cps = toCodepoints(kHan4 + kVariationSelector16);
  ASSERT_EQ(cps.size(), 2u);
  EXPECT_TRUE(utf8IsVariationSelector(cps[1]));
  EXPECT_FALSE(utf8HasCjkBreakOpportunityBetween(cps[0], cps[1]));

  // ni(3) han4(3) VS16(3) hao(3): offset 3 (after ni) and offset 9 (after VS16, before
  // hao) are allowed. Offset 6, between han4 and its selector, must be absent.
  const auto offsets = utf8CjkCharacterBreakByteOffsets(kNi + kHan4 + kVariationSelector16 + kHao);
  EXPECT_EQ(offsets, (std::vector<size_t>{3, 9}));
}

// ParsedText::addWord() is the caller that turns this primitive into full mixed CJK/Latin
// spacing behavior; it needs the Arduino toolchain, unavailable to this host-only binary,
// so this test covers the shared rule addWord() is built on instead.
TEST(CjkBreakOpportunity, MixedChineseAndEnglishBoundary) {
  const auto glued = toCodepoints(kHao + "H");
  ASSERT_EQ(glued.size(), 2u);
  EXPECT_TRUE(utf8HasCjkBreakOpportunityBetween(glued[0], glued[1]));

  const auto pureLatin = toCodepoints(std::string("He"));
  ASSERT_EQ(pureLatin.size(), 2u);
  EXPECT_FALSE(utf8HasCjkBreakOpportunityBetween(pureLatin[0], pureLatin[1]));
}

TEST(CjkBreakOpportunity, HanRunOffersBreakBetweenEveryCharacter) {
  const std::string run = kHan1 + kHan2 + kHan3 + kHan4 + kHan5;
  const auto offsets = utf8CjkCharacterBreakByteOffsets(run);
  EXPECT_EQ(offsets, (std::vector<size_t>{3, 6, 9, 12}));
}

// The every-N fallback must not strand a no-line-start mark, such as an ideographic
// comma, at the top of the next line.
TEST(HyphenatorFallbackGuard, SkipsSplitBeforeNoLineStartMark) {
  const std::string word = kHan1 + kHan2 + kHan3 + kHan4 + kHan5 + kIdeographicComma + kHan6 + kHan7 + kHan8 + kHan9;
  const auto breaks = Hyphenator::breakOffsets(word, /*includeFallback=*/true);

  std::set<size_t> byteOffsets;
  for (const auto& info : breaks) {
    byteOffsets.insert(info.byteOffset);
  }

  // Ten codepoints at 3 bytes each yield candidates 6, 9, 12, 15, 18, 21, 24.
  // 15 sits right before the ideographic comma at codepoint index 5 and must drop out;
  // every other candidate must survive.
  EXPECT_EQ(byteOffsets, (std::set<size_t>{6, 9, 12, 18, 21, 24}));
  EXPECT_EQ(byteOffsets.count(15u), 0u);
}
