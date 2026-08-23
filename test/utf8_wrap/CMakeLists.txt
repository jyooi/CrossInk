add_executable(Utf8WrapTest
  Utf8WrapTest.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(Utf8WrapTest PRIVATE
  ${REPO_ROOT}/lib/Utf8
)

target_link_libraries(Utf8WrapTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(Utf8WrapTest)
