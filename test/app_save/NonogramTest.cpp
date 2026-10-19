#include <gtest/gtest.h>

#include "Nonogram.h"

TEST(Nonogram, ParsesGridAndComputesClues) {
  Nonogram n;
  ASSERT_TRUE(n.parse(".##.##./#######/.#####./..###../...#..."));
  EXPECT_EQ(n.w, 7);
  EXPECT_EQ(n.h, 5);
  uint8_t c[Nonogram::MAX_CLUES];
  ASSERT_EQ(n.clues(0, false, c), 2);  // row 0: 2 2
  EXPECT_EQ(c[0], 2);
  EXPECT_EQ(c[1], 2);
  ASSERT_EQ(n.clues(3, true, c), 1);  // column 3: rows 1-4 filled
  EXPECT_EQ(c[0], 4);
  ASSERT_EQ(n.clues(0, true, c), 1);
  EXPECT_EQ(c[0], 1);
}

TEST(Nonogram, RejectsBadGrids) {
  Nonogram n;
  EXPECT_FALSE(n.parse(""));
  EXPECT_FALSE(n.parse("##/#"));              // ragged
  EXPECT_FALSE(n.parse("################"));  // 16 wide
}

TEST(Nonogram, EmptyLineHasNoClues) {
  Nonogram n;
  ASSERT_TRUE(n.parse("..../.##."));
  uint8_t c[Nonogram::MAX_CLUES];
  EXPECT_EQ(n.clues(0, false, c), 0);
}

TEST(Nonogram, SolvedIgnoresCrosses) {
  Nonogram n;
  ASSERT_TRUE(n.parse("#./.#"));
  EXPECT_FALSE(n.solved());
  n.mark(0, 0) = Nonogram::FILLED;
  n.mark(1, 1) = Nonogram::FILLED;
  n.mark(1, 0) = Nonogram::CROSSED;
  EXPECT_TRUE(n.solved());
  n.mark(0, 1) = Nonogram::FILLED;
  EXPECT_FALSE(n.solved());
}
