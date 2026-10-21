// 2048 for the Pocket SDK: slide with the four arrows, Confirm starts over
// once no move is left. The board and scores are saved on exit.
#include <string.h>

#include "x4.h"

#define N 4

static struct {
  uint8_t cells[N * N];  // exponents: 0 empty, 1 = 2, 11 = 2048
  uint32_t score, best;
} g;
_Static_assert(sizeof(g) == 24, "manifest.json \"save\" must match");

static int canMove(void) {
  for (int r = 0; r < N; ++r)
    for (int c = 0; c < N; ++c) {
      const uint8_t v = g.cells[r * N + c];
      if (!v || (c + 1 < N && g.cells[r * N + c + 1] == v) || (r + 1 < N && g.cells[(r + 1) * N + c] == v)) return 1;
    }
  return 0;
}

static void spawn(void) {
  int empty = 0;
  for (int i = 0; i < N * N; ++i) empty += !g.cells[i];
  if (!empty) return;
  int pick = (int)(x4_rnd() % (uint32_t)empty);
  for (int i = 0; i < N * N; ++i)
    if (!g.cells[i] && pick-- == 0) {
      g.cells[i] = x4_rnd() % 10 == 0 ? 2 : 1;
      return;
    }
}

static void newGame(void) {
  memset(g.cells, 0, sizeof(g.cells));
  g.score = 0;
  spawn();
  spawn();
}

// dir: 0 left, 1 right, 2 up, 3 down. Returns whether any tile moved.
static int slide(int dir) {
  int moved = 0;
  for (int line = 0; line < N; ++line) {
    uint8_t* p[N];
    for (int i = 0; i < N; ++i) {
      const int k = (dir == 0 || dir == 2) ? i : N - 1 - i;  // i = 0 is the edge tiles move to
      p[i] = (dir < 2) ? &g.cells[line * N + k] : &g.cells[k * N + line];
    }
    uint8_t out[N] = {0};
    int n = 0, canMerge = 0;
    for (int i = 0; i < N; ++i) {
      if (!*p[i]) continue;
      if (canMerge && out[n - 1] == *p[i]) {
        g.score += 1u << ++out[n - 1];
        canMerge = 0;
      } else {
        out[n++] = *p[i];
        canMerge = 1;
      }
    }
    for (int i = 0; i < N; ++i) {
      moved |= *p[i] != out[i];
      *p[i] = out[i];
    }
  }
  if (g.score > g.best) g.best = g.score;
  return moved;
}

static void fmt(char* out, const char* label, uint32_t v) {
  char d[12];
  int n = 0;
  do d[n++] = (char)('0' + v % 10);
  while ((v /= 10) > 0);
  while (*label) *out++ = *label++;
  while (n) *out++ = d[--n];
  *out = 0;
}

X4_EXPORT(init) void init(void) {
  if (x4_load(&g, sizeof(g)) != sizeof(g) || !canMove()) {
    const uint32_t best = g.best;
    newGame();
    g.best = best;
  }
}

X4_EXPORT(exit) void exit_(void) { x4_save(&g, sizeof(g)); }

X4_EXPORT(event) int event(int ev) {
  if (!canMove()) {
    if (ev != X4_CONFIRM) return 0;
    const uint32_t best = g.best;
    newGame();
    g.best = best;
    return 1;
  }
  if (ev < X4_LEFT || ev > X4_DOWN || !slide(ev - X4_LEFT)) return 0;
  spawn();
  return 1;
}

X4_EXPORT(render) void render(void) {
  const int th = x4_lang() == 1;
  const int w = x4_width(), side = 24;
  int y = x4_top() + 4;
  char line[32];
  fmt(line, th ? "คะแนน " : "Score ", g.score);
  x4_text(side, y, line, X4_UI12, X4_BOLD);
  fmt(line, th ? "สูงสุด " : "Best ", g.best);
  x4_text(w - side - x4_textw(line, X4_UI12, 0), y, line, X4_UI12, 0);
  y += 40;

  // Dark board, bare empty cells; 2 and 4 white, 8-32 light grey, 64+ black.
  const int board = w - 2 * side, gap = 10, tile = (board - 5 * gap) / 4, lh = x4_lineh(X4_SANS18);
  x4_rfill(side, y, board, board, 14, X4_DARK);
  for (int r = 0; r < N; ++r)
    for (int c = 0; c < N; ++c) {
      const uint8_t v = g.cells[r * N + c];
      if (!v) continue;
      const int tx = side + gap + c * (tile + gap), ty = y + gap + r * (tile + gap), dark = v >= 6;
      x4_rfill(tx, ty, tile, tile, 10, dark ? X4_BLACK : v >= 3 ? X4_LIGHT : X4_WHITE);
      fmt(line, "", 1u << v);
      x4_text(tx + tile / 2, ty + (tile - lh) / 2, line, X4_SANS18, X4_BOLD | X4_CENTER | (dark ? X4_INVERT : 0));
    }
  if (!canMove()) x4_text(w / 2, y + board + 20, th ? "เดินต่อไม่ได้แล้ว" : "No moves left", X4_UI12, X4_BOLD | X4_CENTER);
  x4_hints(0, canMove() ? 0 : (th ? "เกมใหม่" : "New game"), "<", ">");
}
