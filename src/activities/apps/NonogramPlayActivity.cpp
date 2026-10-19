#include "NonogramPlayActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>
#include <Memory.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "apps/TsvCard.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr unsigned long CROSS_HOLD_MS = 600;
constexpr int FAST_REFRESHES_PER_HALF = 20;
constexpr int SIDE = 16;
}  // namespace

void NonogramPlayActivity::onEnter() {
  Activity::onEnter();
  TsvFile file;
  auto row = makeUniqueNoThrow<char[]>(TsvFile::MAX_LINE + 1);
  char* cols[2];
  ok = row && NonogramActivity::openPuzzles(file) && file.read(index, row.get()) &&
       tsv_card::split(row.get(), cols, 2) == 2 && puzzle.parse(cols[1]);
  if (ok) {
    snprintf(name, sizeof(name), "%s", cols[0]);
    NonogramActivity::loadProgress(progress);
    if (progress.current == index) memcpy(puzzle.marks, progress.marks, sizeof(puzzle.marks));
    solved = (progress.solved[index / 32] >> (index % 32)) & 1U && puzzle.solved();
  }
  LOG_INF("NONO", "puzzle %d %s: %dx%d", index, ok ? name : "unreadable", puzzle.w, puzzle.h);
  requestUpdate();
}

void NonogramPlayActivity::onExit() {
  if (dirty) {
    progress.current = static_cast<uint16_t>(index);
    memcpy(progress.marks, puzzle.marks, sizeof(progress.marks));
    if (solved) progress.solved[index / 32] |= 1U << (index % 32);
    const bool saved = NonogramActivity::saveProgress(progress);
    LOG_INF("NONO", "saved puzzle %d%s: %s", index, solved ? " solved" : "", saved ? "ok" : "FAILED");
  }
  Activity::onExit();
}

void NonogramPlayActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  if (!ok) return;
  if (solved) {
    if (mappedInput.wasReleased(Button::Confirm)) finish();
    return;
  }
  const bool cross = mappedInput.wasLongPressed(Button::Confirm, CROSS_HOLD_MS);
  const bool fill = !cross && mappedInput.wasReleased(Button::Confirm);
  int dx = 0, dy = 0;
  if (mappedInput.wasReleased(Button::Left)) dx = -1;
  if (mappedInput.wasReleased(Button::Right)) dx = 1;
  if (mappedInput.wasReleased(Button::Up)) dy = -1;
  if (mappedInput.wasReleased(Button::Down)) dy = 1;
  if (!cross && !fill && dx == 0 && dy == 0) return;
  {
    RenderLock lock;
    uint8_t& m = puzzle.mark(cx, cy);
    if (fill) {
      m = m == Nonogram::FILLED ? Nonogram::EMPTY : Nonogram::FILLED;
      dirty = true;
    } else if (cross) {
      m = m == Nonogram::CROSSED ? Nonogram::EMPTY : Nonogram::CROSSED;
      dirty = true;
    } else {
      cx = (cx + dx + puzzle.w) % puzzle.w;
      cy = (cy + dy + puzzle.h) % puzzle.h;
    }
    solved = puzzle.solved();
  }
  requestUpdate();
}

void NonogramPlayActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageW = renderer.getScreenWidth();
  const int pageH = renderer.getScreenHeight();
  char title[80];
  if (ok && solved) {
    snprintf(title, sizeof(title), "%s", name);
  } else {
    snprintf(title, sizeof(title), "%s #%d", tr(STR_GAME_NONOGRAM), index + 1);
  }
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageW, metrics.headerHeight}, title, nullptr);
  const int top = metrics.topPadding + metrics.headerHeight + 12;
  const int bottom = pageH - metrics.buttonHintsHeight - 40;

  if (!ok) {
    renderer.drawCenteredText(UI_12_FONT_ID, pageH / 2, tr(STR_GAME_NONOGRAM_NONE));
  } else {
    // Clue bands: row clues to the left, column clues stacked above.
    uint8_t c[Nonogram::MAX_CLUES];
    int maxRow = 1, maxCol = 1;
    for (int y = 0; y < puzzle.h; ++y) maxRow = std::max(maxRow, puzzle.clues(y, false, c));
    for (int x = 0; x < puzzle.w; ++x) maxCol = std::max(maxCol, puzzle.clues(x, true, c));
    const int numH = renderer.getLineHeight(UI_10_FONT_ID);
    const int digitW = renderer.getTextWidth(UI_10_FONT_ID, "00") + 4;
    const int rowClueW = maxRow * digitW + 6;
    const int colClueH = maxCol * numH + 6;
    const int cell = std::min((pageW - 2 * SIDE - rowClueW) / puzzle.w, (bottom - top - colClueH) / puzzle.h);
    const int gridW = cell * puzzle.w, gridH = cell * puzzle.h;
    const int gx = (pageW - rowClueW - gridW) / 2 + rowClueW;
    const int gy = top + colClueH;

    char num[4];
    for (int y = 0; y < puzzle.h; ++y) {
      int n = puzzle.clues(y, false, c);
      if (n == 0) c[n++] = 0;  // an empty line shows "0"
      const auto style = y == cy ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
      int x = gx - 6;
      for (int i = n - 1; i >= 0; --i) {
        snprintf(num, sizeof(num), "%u", c[i]);
        x -= digitW;
        const int tw = renderer.getTextWidth(UI_10_FONT_ID, num, style);
        renderer.drawText(UI_10_FONT_ID, x + digitW - tw, gy + y * cell + (cell - numH) / 2, num, true, style);
      }
    }
    for (int x = 0; x < puzzle.w; ++x) {
      int n = puzzle.clues(x, true, c);
      if (n == 0) c[n++] = 0;
      const auto style = x == cx ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR;
      for (int i = 0; i < n; ++i) {
        snprintf(num, sizeof(num), "%u", c[i]);
        const int tw = renderer.getTextWidth(UI_10_FONT_ID, num, style);
        renderer.drawText(UI_10_FONT_ID, gx + x * cell + (cell - tw) / 2, gy - 4 - (n - i) * numH, num, true, style);
      }
    }

    // Cells: filled black, crosses as an X, the cursor a frame inside its cell.
    // Grid lines every cell, heavier every five.
    for (int y = 0; y < puzzle.h; ++y) {
      for (int x = 0; x < puzzle.w; ++x) {
        const int px = gx + x * cell, py = gy + y * cell;
        const uint8_t m = puzzle.mark(x, y);
        if (m == Nonogram::FILLED) renderer.fillRect(px, py, cell, cell, true);
        if (m == Nonogram::CROSSED) {
          renderer.drawLine(px + cell / 4, py + cell / 4, px + cell * 3 / 4, py + cell * 3 / 4, 2, true);
          renderer.drawLine(px + cell * 3 / 4, py + cell / 4, px + cell / 4, py + cell * 3 / 4, 2, true);
        }
        if (x == cx && y == cy && !solved) {
          renderer.drawRect(px + 3, py + 3, cell - 6, cell - 6, 3, m != Nonogram::FILLED);
        }
      }
    }
    for (int x = 0; x <= puzzle.w; ++x) {
      renderer.fillRect(gx + x * cell - (x % 5 == 0 ? 1 : 0), gy, x % 5 == 0 ? 3 : 1, gridH, true);
    }
    for (int y = 0; y <= puzzle.h; ++y) {
      renderer.fillRect(gx, gy + y * cell - (y % 5 == 0 ? 1 : 0), gridW, y % 5 == 0 ? 3 : 1, true);
    }
    if (solved) {
      renderer.drawCenteredText(UI_12_FONT_ID, gy + gridH + 14, tr(STR_GAME_WON), true, EpdFontFamily::BOLD);
    } else {
      renderer.drawCenteredText(UI_10_FONT_ID, gy + gridH + 14, tr(STR_GAME_NONOGRAM_HELP));
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), solved ? tr(STR_GAME_DONE) : tr(STR_GAME_FILL), "<", ">");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (++fastRefreshes >= FAST_REFRESHES_PER_HALF || solved) {
    fastRefreshes = 0;
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  } else {
    renderer.displayBuffer();
  }
}
