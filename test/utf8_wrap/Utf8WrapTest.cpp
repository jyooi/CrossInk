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
