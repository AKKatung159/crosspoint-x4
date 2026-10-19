#include <gtest/gtest.h>

#include <cstdint>

#include "Minesweeper.h"

namespace {
struct Lcg {
  uint32_t s;
  uint32_t operator()() { return s = s * 1664525U + 1013904223U; }
};
using M = Minesweeper;
}  // namespace

TEST(Minesweeper, FirstRevealIsSafeAndOpensArea) {
  for (uint32_t seed = 1; seed < 50; ++seed) {
    M m;
    m.reveal(4, 4, Lcg{seed});
    EXPECT_NE(m.state, M::State::Lost);
    int mines = 0;
    for (const uint8_t c : m.cells) mines += (c & M::MINE) != 0;
    EXPECT_EQ(mines, M::MINES);
    for (int dy = -1; dy <= 1; ++dy)
      for (int dx = -1; dx <= 1; ++dx) EXPECT_FALSE(m.at(4 + dx, 4 + dy) & M::MINE);
    EXPECT_TRUE(m.at(4, 4) & M::OPEN);
    EXPECT_EQ(m.at(4, 4) & M::COUNT, 0);  // no mines around the start, so it flooded
  }
}

TEST(Minesweeper, RevealingAMineLoses) {
  M m;
  m.reveal(0, 0, Lcg{7});
  for (int i = 0; i < M::W * M::H; ++i) {
    if (m.cells[i] & M::MINE) {
      m.reveal(i % M::W, i / M::W, Lcg{7});
      break;
    }
  }
  EXPECT_EQ(m.state, M::State::Lost);
}

TEST(Minesweeper, OpeningEverySafeCellWins) {
  M m;
  m.reveal(0, 0, Lcg{3});
  for (int i = 0; i < M::W * M::H; ++i) {
    if (!(m.cells[i] & M::MINE)) m.reveal(i % M::W, i / M::W, Lcg{3});
  }
  EXPECT_EQ(m.state, M::State::Won);
}

TEST(Minesweeper, FlagsBlockRevealAndToggle) {
  M m;
  m.reveal(0, 0, Lcg{5});
  int closed = -1;
  for (int i = 0; i < M::W * M::H && closed < 0; ++i) {
    if (!(m.cells[i] & M::OPEN)) closed = i;
  }
  ASSERT_GE(closed, 0);
  const int x = closed % M::W, y = closed / M::W;
  m.toggleFlag(x, y);
  EXPECT_EQ(m.flags(), 1);
  m.reveal(x, y, Lcg{5});
  EXPECT_FALSE(m.at(x, y) & M::OPEN);
  m.toggleFlag(x, y);
  EXPECT_EQ(m.flags(), 0);
}

TEST(Minesweeper, StuckRngStillLaysAllMines) {
  M m;
  m.reveal(0, 0, [] { return 0U; });
  int mines = 0;
  for (const uint8_t c : m.cells) mines += (c & M::MINE) != 0;
  EXPECT_EQ(mines, M::MINES);
}
