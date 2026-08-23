#include <gtest/gtest.h>

#include "Utf8.h"

namespace {

int countCodepoints(void* /*ctx*/, const char* text) {
  if (text == nullptr) return 0;
  int count = 0;
  const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text);
  while (utf8NextCodepoint(&cursor) != 0) ++count;
  return count;
}

const Utf8WidthMeasure kCpWidth{countCodepoints, nullptr};

}  // namespace

TEST(Utf8NextWrapUnit, KeepsLatinWordsTogether) {
  EXPECT_EQ(utf8NextWrapUnitBytes("hello world"), 5u);
  EXPECT_EQ(utf8NextWrapUnitBytes("world"), 5u);
}

TEST(Utf8NextWrapUnit, SplitsCjkCharacters) {
  // 中文 is U+4E2D U+6587
  EXPECT_EQ(utf8NextWrapUnitBytes("\xE4\xB8\xAD\xE6\x96\x87"), 3u);
}

TEST(Utf8NextWrapUnit, SplitsBeforeCjkInMixedText) { EXPECT_EQ(utf8NextWrapUnitBytes("Hi\xE4\xB8\xAD"), 2u); }

TEST(Utf8WrapToWidth, WrapsLatinOnSpacesOnly) {
  const auto lines = utf8WrapToWidth("hello world", 5, 2, kCpWidth);
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(lines[0], "hello");
  EXPECT_EQ(lines[1], "world");
}

TEST(Utf8WrapToWidth, KeepsLatinPhraseOnOneLineWhenItFits) {
  const auto lines = utf8WrapToWidth("hello world", 11, 2, kCpWidth);
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_EQ(lines[0], "hello world");
}

TEST(Utf8WrapToWidth, WrapsCjkBetweenCharacters) {
  const auto lines = utf8WrapToWidth("\xE4\xB8\xAD\xE6\x96\x87\xE6\xB8\xB2\xE6\x9F\x93", 2, 3, kCpWidth);
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(lines[0], "\xE4\xB8\xAD\xE6\x96\x87");
  EXPECT_EQ(lines[1], "\xE6\xB8\xB2\xE6\x9F\x93");
}

TEST(Utf8WrapToWidth, DoesNotCollapseMultiLineCjkToOneEllipsis) {
  const auto lines =
      utf8WrapToWidth("\xE4\xB8\xAD\xE6\x96\x87\xE6\xB8\xB2\xE6\x9F\x93\xE6\xB5\x8B\xE8\xAF\x95", 2, 3, kCpWidth);
  ASSERT_EQ(lines.size(), 3u);
  EXPECT_EQ(lines[0], "\xE4\xB8\xAD\xE6\x96\x87");
  EXPECT_EQ(lines[1], "\xE6\xB8\xB2\xE6\x9F\x93");
  EXPECT_EQ(lines[2], "\xE6\xB5\x8B\xE8\xAF\x95");
}

TEST(Utf8WrapToWidth, TruncatesLastCjkLineWithEllipsis) {
  const auto lines = utf8WrapToWidth("\xE4\xB8\xAD\xE6\x96\x87\xE6\xB8\xB2\xE6\x9F\x93", 3, 1, kCpWidth);
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_EQ(lines[0], "\xE4\xB8\xAD\xe2\x80\xa6");
}

// U+300A 《 = "\xE3\x80\x8A", U+300B 》 = "\xE3\x80\x8B", U+3002 。 = "\xE3\x80\x82"
// 红 = "\xE7\xBA\xA2", 楼 = "\xE6\xA5\xBC", 梦 = "\xE6\xA2\xA6"
TEST(Utf8NextWrapUnit, KeepsOpeningPunctuationWithNextCharacter) {
  EXPECT_EQ(utf8NextWrapUnitBytes("\xE3\x80\x8A\xE7\xBA\xA2\xE6\xA5\xBC"), 6u);
}

TEST(Utf8NextWrapUnit, KeepsClosingPunctuationWithPreviousCharacter) {
  EXPECT_EQ(utf8NextWrapUnitBytes("\xE6\xA2\xA6\xE3\x80\x8B"), 6u);
}

TEST(Utf8WrapToWidth, DoesNotStartLineWithClosingBracket) {
  // 《红楼梦》 at a width of four codepoints must not leave 》 alone on line 2.
  const auto lines = utf8WrapToWidth("\xE3\x80\x8A\xE7\xBA\xA2\xE6\xA5\xBC\xE6\xA2\xA6\xE3\x80\x8B", 4, 2, kCpWidth);
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(lines[0], "\xE3\x80\x8A\xE7\xBA\xA2\xE6\xA5\xBC");
  EXPECT_EQ(lines[1], "\xE6\xA2\xA6\xE3\x80\x8B");
}

TEST(Utf8WrapToWidth, DoesNotStartLineWithIdeographicFullStop) {
  // 第一章。开始 wrapped at three codepoints must not put 。 at the line start.
  const auto lines =
      utf8WrapToWidth("\xE7\xAC\xAC\xE4\xB8\x80\xE7\xAB\xA0\xE3\x80\x82\xE5\xBC\x80\xE5\xA7\x8B", 3, 3, kCpWidth);
  ASSERT_EQ(lines.size(), 3u);
  EXPECT_EQ(lines[0], "\xE7\xAC\xAC\xE4\xB8\x80");
  EXPECT_EQ(lines[1], "\xE7\xAB\xA0\xE3\x80\x82\xE5\xBC\x80");
  EXPECT_EQ(lines[2], "\xE5\xA7\x8B");
}

TEST(Utf8WrapToWidth, PreservesRepeatedSpacesInsideALine) {
  const auto lines = utf8WrapToWidth("a  b", 10, 2, kCpWidth);
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_EQ(lines[0], "a  b");
}

TEST(Utf8WrapToWidth, PreservesRepeatedSpacesWhenJoiningTheLastLine) {
  // The last-line branch re-joins the carried word with the rest of the text.
  // Collapsing the run there would make the line measure one column narrower.
  const auto lines = utf8WrapToWidth("aaaa bb  cc", 6, 2, kCpWidth);
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(lines[0], "aaaa");
  EXPECT_EQ(lines[1], "bb  cc");
}

TEST(Utf8WrapToWidth, DropsLeadingSpacesLikeTheOldWordSplit) {
  const auto lines = utf8WrapToWidth("  ab", 10, 2, kCpWidth);
  ASSERT_EQ(lines.size(), 1u);
  EXPECT_EQ(lines[0], "ab");
}

TEST(Utf8WrapToWidth, KeepsTrailingSpaceFromForcingAnEllipsis) {
  const auto lines = utf8WrapToWidth("aaa bbb ", 3, 2, kCpWidth);
  ASSERT_EQ(lines.size(), 2u);
  EXPECT_EQ(lines[0], "aaa");
  EXPECT_EQ(lines[1], "bbb");
}
