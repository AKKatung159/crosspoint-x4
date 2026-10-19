#include <gtest/gtest.h>

#include <string>

#include "TsvCard.h"

namespace {
struct Card {
  bool ok;
  std::string front, back;
};
Card parse(std::string line) {
  const char *f = nullptr, *b = nullptr;
  const bool ok = tsv_card::parse(line.data(), &f, &b);
  return {ok, ok ? f : "", ok ? b : ""};
}
}  // namespace

TEST(TsvCard, SplitsFrontAndBack) {
  const Card c = parse("apple\tแอปเปิล");
  ASSERT_TRUE(c.ok);
  EXPECT_EQ(c.front, "apple");
  EXPECT_EQ(c.back, "แอปเปิล");
}

TEST(TsvCard, DropsCrBomAndExtraColumns) {
  const Card c = parse(
      "\xEF\xBB\xBF"
      "dog\tสุนัข\tnote\r");
  ASSERT_TRUE(c.ok);
  EXPECT_EQ(c.front, "dog");
  EXPECT_EQ(c.back, "สุนัข");
  const Card cr = parse("cat\tแมว\r");
  EXPECT_EQ(cr.back, "แมว");
}

TEST(TsvCard, EscapedNewlineBecomesLineBreak) {
  const Card c = parse("run\tวิ่ง\\nI run every day.");
  ASSERT_TRUE(c.ok);
  EXPECT_EQ(c.back, "วิ่ง\nI run every day.");
}

TEST(TsvCard, SkipsCommentsBlanksAndLinesWithoutTab) {
  EXPECT_FALSE(parse("# English - Thai").ok);
  EXPECT_FALSE(parse("\xEF\xBB\xBF# header").ok);
  EXPECT_FALSE(parse("").ok);
  EXPECT_FALSE(parse("\r").ok);
  EXPECT_FALSE(parse("no tab here").ok);
}

TEST(TsvCard, EmptyBackIsAllowed) {
  const Card c = parse("word\t");
  ASSERT_TRUE(c.ok);
  EXPECT_EQ(c.back, "");
}

TEST(TsvCard, SplitsColumns) {
  std::string row =
      "\xEF\xBB\xBF"
      "00_Fool\tThe Fool\t\tNew beginnings";
  char* cols[7];
  ASSERT_EQ(tsv_card::split(row.data(), cols, 7), 4);
  EXPECT_STREQ(cols[0], "00_Fool");
  EXPECT_STREQ(cols[2], "");
  EXPECT_STREQ(cols[3], "New beginnings");
  std::string capped = "a\tb\tc";
  ASSERT_EQ(tsv_card::split(capped.data(), cols, 2), 2);
  EXPECT_STREQ(cols[1], "b\tc");
}
