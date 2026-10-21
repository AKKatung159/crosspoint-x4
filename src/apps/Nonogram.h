#pragma once

#include <cstdint>
#include <cstring>

// PERSONAL: Nonogram (picross) rules, free of HAL types for the host tests.
// A puzzle row in puzzles.tsv is "name<TAB>grid", the grid's rows separated by
// '/', '#' for a filled cell and anything else for blank, e.g.
//   Heart<TAB>.##.##./#######/.#####./..###../...#...
// Clues are computed from the solution, so a puzzle file needs no numbers.
struct Nonogram {
  static constexpr int MAX = 15;
  static constexpr int MAX_CLUES = (MAX + 1) / 2;
  enum Mark : uint8_t { EMPTY, FILLED, CROSSED };

  int w = 0, h = 0;
  uint16_t solution[MAX] = {};  // bit x of row y
  uint8_t marks[MAX * MAX] = {};

  // False for an empty, ragged or oversized grid.
  bool parse(const char* grid) {
    w = h = 0;
    memset(solution, 0, sizeof(solution));
    memset(marks, 0, sizeof(marks));
    int x = 0;
    for (const char* p = grid;; ++p) {
      if (*p == '/' || *p == '\0') {
        if (h == 0) w = x;
        if (x != w || w == 0 || h == MAX) return false;
        ++h;
        x = 0;
        if (*p == '\0') return true;
        continue;
      }
      if (*p == '\r' || *p == '\n') continue;
      if (x == MAX) return false;
      if (*p == '#') solution[h] |= static_cast<uint16_t>(1U << x);
      ++x;
    }
  }

  bool filled(const int x, const int y) const { return (solution[y] >> x) & 1U; }
  uint8_t& mark(const int x, const int y) { return marks[y * MAX + x]; }
  uint8_t mark(const int x, const int y) const { return marks[y * MAX + x]; }

  // Run lengths of row `y` (or column `x` when column is true) into out;
  // returns the count, 0 for an empty line.
  int clues(const int index, const bool column, uint8_t out[MAX_CLUES]) const {
    int n = 0, run = 0;
    const int len = column ? h : w;
    for (int i = 0; i <= len; ++i) {
      const bool on = i < len && (column ? filled(index, i) : filled(i, index));
      if (on) {
        ++run;
      } else if (run > 0) {
        out[n++] = static_cast<uint8_t>(run);
        run = 0;
      }
    }
    return n;
  }

  // Solved when the FILLED marks match the solution exactly (crosses are notes).
  bool solved() const {
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        if ((mark(x, y) == FILLED) != filled(x, y)) return false;
      }
    }
    return true;
  }
};
