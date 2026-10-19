add_executable(AppSaveFormatTest
  AppSaveFormatTest.cpp
)

target_include_directories(AppSaveFormatTest PRIVATE
  ${REPO_ROOT}/src/apps
)

target_link_libraries(AppSaveFormatTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(AppSaveFormatTest)
