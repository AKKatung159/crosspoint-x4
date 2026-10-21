#pragma once

#include <cstdint>
#include <cstring>

// PERSONAL: Minesweeper rules, free of HAL types for the host tests. Mines are
// laid on the first reveal, away from that cell and its neighbours, so the
// first move never loses.
struct Minesweeper {
  static constexpr int W = 9;
  static constexpr int H = 9;
  static constexpr int MINES = 10;
  enum : uint8_t { MINE = 0x10, OPEN = 0x20, FLAG = 0x40, COUNT = 0x0F };
  enum class State : uint8_t { Fresh, Playing, Won, Lost };

  uint8_t cells[W * H] = {};
  State state = State::Fresh;

  uint8_t& at(const int x, const int y) { return cells[y * W + x]; }
  uint8_t at(const int x, const int y) const { return cells[y * W + x]; }
  static bool inside(const int x, const int y) { return x >= 0 && x < W && y >= 0 && y < H; }

  // `rng` returns uniform 32-bit values (esp_random on the device).
  template <typename Rng>
  void reveal(const int x, const int y, Rng&& rng) {
    if (!inside(x, y) || state == State::Won || state == State::Lost) return;
    if (state == State::Fresh) {
      layMines(x, y, rng);
      state = State::Playing;
    }
    if (at(x, y) & (OPEN | FLAG)) return;
    if (at(x, y) & MINE) {
      at(x, y) |= OPEN;
      state = State::Lost;
      return;
    }
    // Open the cell and flood through empty (count 0) neighbours. Each cell is
    // pushed at most once (marked OPEN when pushed), so W * H slots suffice.
    uint8_t stack[W * H];
    int top = 0;
    at(x, y) |= OPEN;
    stack[top++] = static_cast<uint8_t>(y * W + x);
    while (top > 0) {
      const int i = stack[--top];
      if ((cells[i] & COUNT) != 0) continue;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          const int nx = i % W + dx, ny = i / W + dy;
          if (!inside(nx, ny) || (at(nx, ny) & (OPEN | FLAG | MINE))) continue;
          at(nx, ny) |= OPEN;
          stack[top++] = static_cast<uint8_t>(ny * W + nx);
        }
      }
    }
    int closedSafe = 0;
    for (const uint8_t c : cells) closedSafe += !(c & (OPEN | MINE));
    if (closedSafe == 0) state = State::Won;
  }

  void toggleFlag(const int x, const int y) {
    if (inside(x, y) && state != State::Won && state != State::Lost && !(at(x, y) & OPEN)) at(x, y) ^= FLAG;
  }

  int flags() const {
    int n = 0;
    for (const uint8_t c : cells) n += (c & FLAG) != 0;
    return n;
  }

  void reset() {
    memset(cells, 0, sizeof(cells));
    state = State::Fresh;
  }

 private:
  template <typename Rng>
  void layMines(const int sx, const int sy, Rng& rng) {
    // A random start, then the next free cell: always terminates, even with a
    // stuck or weak RNG.
    for (int placed = 0; placed < MINES; ++placed) {
      int i = static_cast<int>(rng() % (W * H));
      for (;; i = (i + 1) % (W * H)) {
        const int x = i % W, y = i / W;
        const bool nearStart = x >= sx - 1 && x <= sx + 1 && y >= sy - 1 && y <= sy + 1;
        if (!nearStart && !(cells[i] & MINE)) break;
      }
      cells[i] |= MINE;
    }
    for (int y = 0; y < H; ++y) {
      for (int x = 0; x < W; ++x) {
        int n = 0;
        for (int dy = -1; dy <= 1; ++dy) {
          for (int dx = -1; dx <= 1; ++dx) n += inside(x + dx, y + dy) && (at(x + dx, y + dy) & MINE);
        }
        at(x, y) |= static_cast<uint8_t>(n);
      }
    }
  }
};
