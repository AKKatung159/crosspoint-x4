# Thai support (lib/Thai): word breaking, shaping, justification policy.
add_executable(ThaiTest
  ThaiBrkTest.cpp
  ThaiShapingTest.cpp
  ThaiJustifyTest.cpp
  ${REPO_ROOT}/lib/Thai/ThaiBrk.cpp
  ${REPO_ROOT}/lib/Thai/ThaiShaping.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)

target_include_directories(ThaiTest PRIVATE
  ${REPO_ROOT}/lib/Thai
  ${REPO_ROOT}/lib/Utf8
)

target_link_libraries(ThaiTest PRIVATE
  crosspoint_test_common
  GTest::gtest_main
)

gtest_discover_tests(ThaiTest)

# Small CLI used by scripts/thai/eval_segmentation.py: reads UTF-8 lines on
# stdin and prints them with '|' at every break opportunity.
add_executable(thai_seg_cli
  thai_seg_cli.cpp
  ${REPO_ROOT}/lib/Thai/ThaiBrk.cpp
  ${REPO_ROOT}/lib/Utf8/Utf8.cpp
)
target_include_directories(thai_seg_cli PRIVATE ${REPO_ROOT}/lib/Thai ${REPO_ROOT}/lib/Utf8)
