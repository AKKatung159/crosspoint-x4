#pragma once

#include <cstdint>
#include <cstring>

// PERSONAL: 2048 game rules, free of HAL types for the host tests. Cells hold
// exponents: 0 is empty, 1 is the tile 2, 11 is 2048.
struct Game2048 {
  static constexpr int N = 4;
  enum Dir : uint8_t { LEFT, RIGHT, UP, DOWN };

  uint8_t cells[N * N] = {};
  uint32_t score = 0;
  uint32_t best = 0;

  uint8_t& at(const int row, const int col) { return cells[row * N + col]; }
  uint8_t at(const int row, const int col) const { return cells[row * N + col]; }

  // Slides and merges toward `dir`. Returns whether any tile moved.
  bool move(const Dir dir) {
    bool moved = false;
    for (int line = 0; line < N; ++line) {
      uint8_t* ptrs[N];
      for (int i = 0; i < N; ++i) {
        // i = 0 is the edge the tiles move toward.
        const int k = (dir == LEFT || dir == UP) ? i : N - 1 - i;
        ptrs[i] = (dir == LEFT || dir == RIGHT) ? &at(line, k) : &at(k, line);
      }
      uint8_t out[N] = {};
      int n = 0;
      bool canMerge = false;
      for (const uint8_t* p : ptrs) {
        if (*p == 0) continue;
        if (canMerge && out[n - 1] == *p) {
          ++out[n - 1];
          score += 1U << out[n - 1];
          canMerge = false;
        } else {
          out[n++] = *p;
          canMerge = true;
        }
      }
      for (int i = 0; i < N; ++i) {
        moved |= *ptrs[i] != out[i];
        *ptrs[i] = out[i];
      }
    }
    if (score > best) best = score;
    return moved;
  }

  bool canMove() const {
    for (int r = 0; r < N; ++r) {
      for (int c = 0; c < N; ++c) {
        const uint8_t v = at(r, c);
        if (v == 0 || (c + 1 < N && at(r, c + 1) == v) || (r + 1 < N && at(r + 1, c) == v)) return true;
      }
    }
    return false;
  }

  bool won() const {
    for (const uint8_t v : cells) {
      if (v >= 11) return true;
    }
    return false;
  }

  // Puts a 2 (or a 4 when roll % 10 == 0) on the empty cell number `pick % empties`.
  void spawn(const uint32_t pick, const uint32_t roll) {
    int empties = 0;
    for (const uint8_t v : cells) empties += v == 0;
    if (empties == 0) return;
    int target = static_cast<int>(pick % static_cast<uint32_t>(empties));
    for (uint8_t& v : cells) {
      if (v == 0 && target-- == 0) {
        v = roll % 10 == 0 ? 2 : 1;
        return;
      }
    }
  }

  void reset() {
    memset(cells, 0, sizeof(cells));
    score = 0;
  }
};
