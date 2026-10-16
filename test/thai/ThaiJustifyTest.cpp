#include <ThaiJustify.h>
#include <gtest/gtest.h>

using ThaiJustify::distribute;

TEST(ThaiJustify, NoThaiGapsIsTheUpstreamEvenSplit) {
  EXPECT_EQ(distribute(30, 0, 6).otherGap, 5);
  EXPECT_EQ(distribute(30, 0, 6).thaiGap, 0);
}

TEST(ThaiJustify, SpacesTakeTheStretchFirst) {
  const auto e = distribute(20, 10, 2);  // 10 Thai word gaps, 2 real spaces
  EXPECT_EQ(e.thaiGap, 1);
  EXPECT_EQ(e.otherGap, 5);
}

TEST(ThaiJustify, SpacesAreCappedAndThaiGapsShareTheRest) {
  const auto e = distribute(60, 10, 1);
  EXPECT_EQ(e.otherGap, ThaiJustify::kMaxSpaceStretchPx);
  EXPECT_EQ(e.thaiGap, (60 - ThaiJustify::kMaxSpaceStretchPx) / 10);
  EXPECT_LE(e.otherGap + 10 * e.thaiGap, 60);
}

TEST(ThaiJustify, LineWithoutSpacesStretchesThaiGaps) {
  EXPECT_EQ(distribute(24, 8, 0).thaiGap, 3);
  EXPECT_EQ(distribute(0, 8, 0).thaiGap, 0);
  EXPECT_EQ(distribute(-5, 8, 3).thaiGap, 0);
}

TEST(ThaiJustify, ThaiWordGapNeedsThaiOnBothSides) {
  EXPECT_TRUE(ThaiJustify::isThaiWordGap(true, 0x0E01, 0x0E02));
  EXPECT_FALSE(ThaiJustify::isThaiWordGap(false, 0x0E01, 0x0E02));  // a real space
  EXPECT_FALSE(ThaiJustify::isThaiWordGap(true, 'A', 0x0E02));      // script transition
  EXPECT_FALSE(ThaiJustify::isThaiWordGap(true, 0x4E00, 0x4E01));   // CJK gap
}
