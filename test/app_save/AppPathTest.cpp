#include <gtest/gtest.h>

#include "sdk/AppPath.h"

TEST(AppPath, AcceptsPlainRelativePaths) {
  EXPECT_TRUE(app_path::valid("deck.tsv"));
  EXPECT_TRUE(app_path::valid("cards/00_Fool.bmp"));
  EXPECT_TRUE(app_path::valid(".hidden"));
  EXPECT_TRUE(app_path::valid("a...b"));
}

TEST(AppPath, RejectsEscapesAndOddPaths) {
  EXPECT_FALSE(app_path::valid(""));
  EXPECT_FALSE(app_path::valid("/etc"));
  EXPECT_FALSE(app_path::valid(".."));
  EXPECT_FALSE(app_path::valid("../x"));
  EXPECT_FALSE(app_path::valid("cards/../../x"));
  EXPECT_FALSE(app_path::valid("cards/./x"));
  EXPECT_FALSE(app_path::valid("a//b"));
  EXPECT_FALSE(app_path::valid("a/"));
  EXPECT_FALSE(app_path::valid("a\\b"));
  EXPECT_FALSE(app_path::valid("ไทย.txt"));
  EXPECT_FALSE(app_path::valid(std::string(64, 'a').c_str()));
  EXPECT_TRUE(app_path::valid(std::string(63, 'a').c_str()));
}

TEST(AppPath, ResolvesInsideTheAppFolder) {
  char out[96];
  ASSERT_TRUE(app_path::resolve("/apps/tarot", "cards/x.bmp", out, sizeof(out)));
  EXPECT_STREQ(out, "/apps/tarot/cards/x.bmp");
  EXPECT_FALSE(app_path::resolve("/apps/tarot", "../2048/save", out, sizeof(out)));
}
