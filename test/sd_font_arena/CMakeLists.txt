add_executable(GlyphArenaIntervalsTest
  GlyphArenaIntervalsTest.cpp
  AdvanceCacheLimitTest.cpp
)

target_include_directories(GlyphArenaIntervalsTest PRIVATE
  ${REPO_ROOT}/lib/EpdFont
)

target_link_libraries(GlyphArenaIntervalsTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(GlyphArenaIntervalsTest)
