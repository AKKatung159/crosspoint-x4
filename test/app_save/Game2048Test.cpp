#include <gtest/gtest.h>

#include "Game2048.h"

namespace {
Game2048 withRow(std::initializer_list<uint8_t> row) {
  Game2048 g;
  int c = 0;
  for (const uint8_t v : row) g.at(0, c++) = v;
  return g;
}
}  // namespace

TEST(Game2048, MergesPairsOnceTowardTheEdge) {
  Game2048 g = withRow({1, 1, 1, 1});  // 2 2 2 2
  ASSERT_TRUE(g.move(Game2048::LEFT));
  EXPECT_EQ(g.at(0, 0), 2);
  EXPECT_EQ(g.at(0, 1), 2);
  EXPECT_EQ(g.at(0, 2), 0);
  EXPECT_EQ(g.score, 8U);
}

TEST(Game2048, MergedTileDoesNotMergeAgain) {
  Game2048 g = withRow({2, 1, 1, 0});  // 4 2 2 -> 4 4, not 8
  g.move(Game2048::LEFT);
  EXPECT_EQ(g.at(0, 0), 2);
  EXPECT_EQ(g.at(0, 1), 2);
}

TEST(Game2048, RightMovesTowardTheRightEdge) {
  Game2048 g = withRow({1, 0, 1, 2});  // 2 _ 2 4 -> _ _ 4 4
  g.move(Game2048::RIGHT);
  EXPECT_EQ(g.at(0, 3), 2);
  EXPECT_EQ(g.at(0, 2), 2);
  EXPECT_EQ(g.at(0, 1), 0);
}

TEST(Game2048, UpAndDownWorkOnColumns) {
  Game2048 g;
  g.at(3, 0) = 1;
  g.at(1, 0) = 1;
  ASSERT_TRUE(g.move(Game2048::UP));
  EXPECT_EQ(g.at(0, 0), 2);
  ASSERT_TRUE(g.move(Game2048::DOWN));
  EXPECT_EQ(g.at(3, 0), 2);
}

TEST(Game2048, NoMoveReportsFalse) {
  Game2048 g = withRow({1, 2, 0, 0});
  EXPECT_FALSE(g.move(Game2048::LEFT));
}

TEST(Game2048, DetectsGameOverAndWin) {
  Game2048 g;
  for (int i = 0; i < 16; ++i) g.cells[i] = static_cast<uint8_t>(i % 2 + (i / 4) % 2 * 2 + 1);  // no equal neighbours
  EXPECT_FALSE(g.canMove());
  g.cells[5] = 0;
  EXPECT_TRUE(g.canMove());
  EXPECT_FALSE(g.won());
  g.cells[0] = 11;
  EXPECT_TRUE(g.won());
}

TEST(Game2048, SpawnFillsTheChosenEmptyCell) {
  Game2048 g;
  g.spawn(5, 1);
  EXPECT_EQ(g.cells[5], 1);
  g.spawn(0, 10);  // roll % 10 == 0 -> a 4
  EXPECT_EQ(g.cells[0], 2);
}
