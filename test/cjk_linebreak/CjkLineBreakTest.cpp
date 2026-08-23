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
const std::string kIdeographicPeriod = "\xE3\x80\x82";    // U+3002, no-line-start
const std::string kOpenCornerBracket = "\xE3\x80\x8C";    // U+300C, no-line-end
const std::string kCloseCornerBracket = "\xE3\x80\x8D";   // U+300D, no-line-start
const std::string kEllipsis = "\xE2\x80\xA6";             // U+2026
const std::string kEmDash = "\xE2\x80\x94";               // U+2014
const std::string kReversedQuote = "\xE3\x80\x9D";        // U+301D, no-line-end
const std::string kVariationSelector16 = "\xEF\xB8\x8F";  // U+FE0F
const std::string kSmallKatakanaKe = "\xE3\x83\xB6";      // U+30F6, small kana
const std::string kYear = "\xE5\xB9\xB4";                 // U+5E74
const std::string kMonth = "\xE6\x9C\x88";                // U+6708
const std::string kDay = "\xE6\x97\xA5";                  // U+65E5
const std::string kFullwidthComma = "\xEF\xBC\x8C";       // U+FF0C, no-line-start

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

// Small katakana ke is the counter in a word such as three-months. A line must not
// begin with it, so there is no break opportunity between it and the Han before it.
TEST(CjkBreakOpportunity, SmallKanaKeCannotOpenALine) {
  const auto cps = toCodepoints(kHan3 + kSmallKatakanaKe + kMonth);
  ASSERT_EQ(cps.size(), 3u);
  EXPECT_TRUE(utf8IsNoLineStartMark(cps[1]));
  EXPECT_FALSE(utf8HasCjkBreakOpportunityBetween(cps[0], cps[1]));

  const auto offsets = utf8CjkCharacterBreakByteOffsets(kHan3 + kSmallKatakanaKe + kMonth);
  EXPECT_EQ(offsets, (std::vector<size_t>{6}));
}

// JIS-sourced Japanese EPUBs write the range dash as U+301C WAVE DASH, not as the
// fullwidth tilde. Both must be blocked from opening a line.
TEST(CjkBreakOpportunity, WaveDashCannotOpenALine) {
  const std::string waveDash = "\xE3\x80\x9C";  // U+301C
  const auto cps = toCodepoints(kHan1 + waveDash + kHan2);
  ASSERT_EQ(cps.size(), 3u);
  EXPECT_TRUE(utf8IsNoLineStartMark(cps[1]));
  EXPECT_FALSE(utf8HasCjkBreakOpportunityBetween(cps[0], cps[1]));

  EXPECT_EQ(utf8CjkCharacterBreakByteOffsets(kHan1 + waveDash + kHan2), (std::vector<size_t>{6}));
}

// Halfwidth small katakana attach to the character before them the same way the
// fullwidth small kana do.
TEST(CjkBreakOpportunity, HalfwidthSmallKatakanaCannotOpenALine) {
  const std::string halfwidthKa = "\xEF\xBD\xB6";        // U+FF76, a normal halfwidth katakana
  const std::string halfwidthSmallTsu = "\xEF\xBD\xAF";  // U+FF6F

  const auto cps = toCodepoints(halfwidthKa + halfwidthSmallTsu);
  ASSERT_EQ(cps.size(), 2u);
  EXPECT_FALSE(utf8IsNoLineStartMark(cps[0]));
  EXPECT_TRUE(utf8IsNoLineStartMark(cps[1]));
  EXPECT_FALSE(utf8HasCjkBreakOpportunityBetween(cps[0], cps[1]));

  // U+FF67 and U+FF6F are the ends of the halfwidth small-katakana range.
  EXPECT_TRUE(utf8IsNoLineStartMark(0xFF67));
  EXPECT_TRUE(utf8IsNoLineStartMark(0xFF6F));
  EXPECT_FALSE(utf8IsNoLineStartMark(0xFF66));
  EXPECT_FALSE(utf8IsNoLineStartMark(0xFF70 + 1));
}

TEST(CjkBreakOpportunity, HanRunOffersBreakBetweenEveryCharacter) {
  const std::string run = kHan1 + kHan2 + kHan3 + kHan4 + kHan5;
  const auto offsets = utf8CjkCharacterBreakByteOffsets(run);
  EXPECT_EQ(offsets, (std::vector<size_t>{3, 6, 9, 12}));
}

TEST(CjkIndentDefault, MajorityCjkTextDetection) {
  EXPECT_TRUE(utf8IsMajorityCjkText(kNi + kHao + kIdeographicComma + kNi + kHao));
  EXPECT_FALSE(utf8IsMajorityCjkText("Hello, world"));
  EXPECT_FALSE(utf8IsMajorityCjkText(""));
  // A short English aside inside a mostly-Chinese sentence still counts as CJK majority.
  EXPECT_TRUE(utf8IsMajorityCjkText(kNi + kHao + std::string("OK") + kNi + kHao + kNi + kHao));
}

// Brackets, the ideographic period, and spaces are punctuation, not letters. A short
// line of dialogue must not be judged non-CJK just because punctuation outnumbers Han.
TEST(CjkIndentDefault, PunctuationIsExcludedFromTheMajorityDenominator) {
  Utf8CjkTextStats stats;
  utf8AccumulateCjkTextStats(kOpenCornerBracket + kHan1 + kIdeographicPeriod + kCloseCornerBracket, stats);
  EXPECT_EQ(stats.letters, 1u);
  EXPECT_EQ(stats.hanOrKana, 1u);
}

// Too few letters to judge from content. The caller must fall back to the book language,
// which is how a three-character bracketed line still gets the CJK indent in a zh book.
TEST(CjkIndentDefault, ShortDialogueParagraphIsUndetermined) {
  Utf8CjkTextStats stats;
  utf8AccumulateCjkTextStats(kOpenCornerBracket + kHan1 + kIdeographicPeriod + kCloseCornerBracket, stats);
  EXPECT_EQ(utf8ClassifyCjkMajority(stats), Utf8CjkMajority::Undetermined);
  EXPECT_TRUE(utf8IsCjkLanguageTag("zh"));
}

// A paragraph is classified from all of its words together, the way ParsedText feeds it.
TEST(CjkIndentDefault, StatsAccumulateAcrossWords) {
  Utf8CjkTextStats stats;
  for (const std::string& word : {kNi, kHao, std::string("and"), kHan1, kHan2}) {
    utf8AccumulateCjkTextStats(word, stats);
  }
  EXPECT_EQ(stats.letters, 7u);
  EXPECT_EQ(stats.hanOrKana, 4u);
  EXPECT_EQ(utf8ClassifyCjkMajority(stats), Utf8CjkMajority::Cjk);
}

// The widened no-line-start list is a line-break rule only. Justification keeps its own
// closing-punctuation list, so the gap before a Latin em dash still stretches.
TEST(JustifyClosingPunctuation, ExcludesMarksThatAreOnlyLineBreakRules) {
  const auto emDash = toCodepoints(kEmDash).front();
  EXPECT_TRUE(utf8IsNoLineStartMark(emDash));
  EXPECT_FALSE(utf8IsJustifyClosingPunctuation(emDash));

  const auto ellipsis = toCodepoints(kEllipsis).front();
  EXPECT_TRUE(utf8IsNoLineStartMark(ellipsis));
  EXPECT_FALSE(utf8IsJustifyClosingPunctuation(ellipsis));

  const auto middleDot = toCodepoints("\xC2\xB7").front();  // U+00B7
  EXPECT_TRUE(utf8IsNoLineStartMark(middleDot));
  EXPECT_FALSE(utf8IsJustifyClosingPunctuation(middleDot));

  const auto smallTsu = toCodepoints("\xE3\x81\xA3").front();  // U+3063
  EXPECT_TRUE(utf8IsNoLineStartMark(smallTsu));
  EXPECT_FALSE(utf8IsJustifyClosingPunctuation(smallTsu));
}

TEST(JustifyClosingPunctuation, KeepsRealClosingMarks) {
  EXPECT_TRUE(utf8IsJustifyClosingPunctuation('.'));
  EXPECT_TRUE(utf8IsJustifyClosingPunctuation(')'));
  EXPECT_TRUE(utf8IsJustifyClosingPunctuation(toCodepoints(kIdeographicPeriod).front()));
  EXPECT_TRUE(utf8IsJustifyClosingPunctuation(toCodepoints(kCloseCornerBracket).front()));
  EXPECT_FALSE(utf8IsJustifyClosingPunctuation(toCodepoints(kHan1).front()));
}

// A digit is not evidence of script. A Chinese paragraph that leads with a date must
// still classify as CJK, otherwise it indents differently from its neighbours.
TEST(CjkIndentDefault, AsciiDigitsDoNotDiluteTheMajority) {
  Utf8CjkTextStats stats;
  utf8AccumulateCjkTextStats(std::string("2024") + kYear + "1" + kMonth + "1" + kDay + kFullwidthComma + kNi + kHao +
                                 kHan1 + kIdeographicPeriod,
                             stats);
  EXPECT_EQ(stats.letters, 6u);
  EXPECT_EQ(stats.hanOrKana, 6u);
  EXPECT_EQ(utf8ClassifyCjkMajority(stats), Utf8CjkMajority::Cjk);
}

TEST(CjkIndentDefault, LanguageTagDetection) {
  EXPECT_TRUE(utf8IsCjkLanguageTag("zh"));
  EXPECT_TRUE(utf8IsCjkLanguageTag("zh-Hans"));
  EXPECT_TRUE(utf8IsCjkLanguageTag("ZH-TW"));
  EXPECT_TRUE(utf8IsCjkLanguageTag("ja"));
  EXPECT_FALSE(utf8IsCjkLanguageTag("en"));
  EXPECT_FALSE(utf8IsCjkLanguageTag("ko"));
  EXPECT_FALSE(utf8IsCjkLanguageTag(""));
}

// EPUB 2 metadata often carries the ISO 639-2 three-letter code instead of the
// two-letter one. Both forms must pick the CJK indent.
TEST(CjkIndentDefault, LanguageTagAcceptsIso639_2Codes) {
  EXPECT_TRUE(utf8IsCjkLanguageTag("zho"));
  EXPECT_TRUE(utf8IsCjkLanguageTag("chi"));
  EXPECT_TRUE(utf8IsCjkLanguageTag("jpn"));
  EXPECT_TRUE(utf8IsCjkLanguageTag("ZHO-Hant"));
  EXPECT_FALSE(utf8IsCjkLanguageTag("eng"));
  EXPECT_FALSE(utf8IsCjkLanguageTag("kor"));
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

// The apostrophe path runs a second every-N fallback, inside appendSegmentPatternBreaks.
// It must honour the same rules: the candidate that would strand the interior period at
// the top of the next line has to drop out, while the other candidate survives.
TEST(HyphenatorFallbackGuard, SegmentFallbackSkipsSplitBeforeNoLineStartMark) {
  Hyphenator::setPreferredLanguage("fr");
  const auto breaks = Hyphenator::breakOffsets("d\'aa.aa", /*includeFallback=*/true);
  Hyphenator::setPreferredLanguage("");

  std::set<size_t> byteOffsets;
  for (const auto& info : breaks) {
    byteOffsets.insert(info.byteOffset);
  }

  // Segment "aa.aa" starts at byte 2. Candidates are segment index 2 (byte 4, the period)
  // and segment index 3 (byte 5). Only the period candidate is illegal.
  EXPECT_EQ(byteOffsets.count(4u), 0u);
  EXPECT_EQ(byteOffsets.count(5u), 1u);
}

// The every-N fallback must obey every rule utf8HasCjkBreakOpportunityBetween applies,
// not just the two kinsoku ones. A variation selector picks the glyph shape of the
// character before it, so a split between the pair leaves a bare glyph on one line and
// a zero-width selector opening the next.
TEST(HyphenatorFallbackGuard, FallbackNeverSplitsAVariationSelectorFromItsBase) {
  const std::string word =
      "abc\xE2\x9C\x94\xEF\xB8\x8F"
      "defghij";  // U+2714 then U+FE0F

  const auto breaks = Hyphenator::breakOffsets(word, /*includeFallback=*/true);

  std::set<size_t> byteOffsets;
  for (const auto& info : breaks) {
    byteOffsets.insert(info.byteOffset);
  }

  // Byte 6 is the variation selector; byte 3 is its base and stays a legal split.
  EXPECT_EQ(byteOffsets.count(6u), 0u);
  EXPECT_EQ(byteOffsets.count(3u), 1u);
}

// A combining mark that the composition table cannot precompose must stay attached the
// same way.
TEST(HyphenatorFallbackGuard, FallbackNeverSplitsACombiningMarkFromItsBase) {
  const std::string word =
      "abcx\xE2\x83\x9D"
      "defgh";  // U+20DD combining enclosing circle

  const auto breaks = Hyphenator::breakOffsets(word, /*includeFallback=*/true);

  std::set<size_t> byteOffsets;
  for (const auto& info : breaks) {
    byteOffsets.insert(info.byteOffset);
  }

  EXPECT_EQ(byteOffsets.count(4u), 0u);
  EXPECT_EQ(byteOffsets.count(3u), 1u);
}

// Kinsoku must not make a token unsplittable. Every interior candidate in a run of
// ideographic periods sits before a no-line-start mark, so the guard alone would drop
// them all and the run would overflow the line instead of wrapping. The run is under
// PATHOLOGICAL_TOKEN_MIN_BYTES, so the pathological-token splitter does not cover it.
TEST(HyphenatorFallbackGuard, KeepsCandidatesWhenTheGuardWouldDropThemAll) {
  std::string word;
  for (int i = 0; i < 8; ++i) word += kIdeographicPeriod;

  const auto breaks = Hyphenator::breakOffsets(word, /*includeFallback=*/true);
  EXPECT_FALSE(breaks.empty());
}
