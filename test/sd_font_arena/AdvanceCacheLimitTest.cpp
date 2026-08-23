#include <gtest/gtest.h>

#include "SdCardFont.h"

TEST(AdvanceCacheLimit, LatinFamiliesStayAt256) {
  EXPECT_EQ(SdCardFont::advanceCacheLimitFor(false), 256u);
  EXPECT_EQ(SdCardFont::ADVANCE_CACHE_LIMIT, 256u);
}

TEST(AdvanceCacheLimit, CjkFamiliesUse1024) {
  EXPECT_EQ(SdCardFont::advanceCacheLimitFor(true), 1024u);
  EXPECT_EQ(SdCardFont::ADVANCE_CACHE_LIMIT_CJK, 1024u);
}
