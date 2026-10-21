add_executable(PocketAppsTest
  AppSaveFormatTest.cpp
  TsvCardTest.cpp
  Game2048Test.cpp
  MinesweeperTest.cpp
  NonogramTest.cpp
  AppPathTest.cpp
)

target_include_directories(PocketAppsTest PRIVATE
  ${REPO_ROOT}/src/apps
)

target_link_libraries(PocketAppsTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(PocketAppsTest)
