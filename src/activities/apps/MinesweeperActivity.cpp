#include "MinesweeperActivity.h"

#include <Arduino.h>
#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "apps/AppSave.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char APP_ID[] = "minesweeper";
constexpr uint16_t SAVE_VERSION = 1;
constexpr unsigned long FLAG_HOLD_MS = 600;
constexpr int FAST_REFRESHES_PER_HALF = 20;

struct Saved {
  uint8_t cells[Minesweeper::W * Minesweeper::H];
  uint8_t state;
  uint8_t cx, cy;
};

uint32_t hwRandom() { return esp_random(); }
}  // namespace

void MinesweeperActivity::onEnter() {
  Activity::onEnter();
  Saved s;
  if (app_save::load(APP_ID, SAVE_VERSION, &s, sizeof(s)) &&
      s.state <= static_cast<uint8_t>(Minesweeper::State::Lost) && Minesweeper::inside(s.cx, s.cy)) {
    memcpy(game.cells, s.cells, sizeof(s.cells));
    game.state = static_cast<Minesweeper::State>(s.state);
    cx = s.cx;
    cy = s.cy;
  }
  requestUpdate();
}

void MinesweeperActivity::onExit() {
  if (dirty) {
    Saved s;
    memcpy(s.cells, game.cells, sizeof(s.cells));
    s.state = static_cast<uint8_t>(game.state);
    s.cx = static_cast<uint8_t>(cx);
    s.cy = static_cast<uint8_t>(cy);
    const bool ok = app_save::save(APP_ID, SAVE_VERSION, &s, sizeof(s), sizeof(s));
    LOG_INF("MINES", "saved state %d: %s", s.state, ok ? "ok" : "FAILED");
  }
  Activity::onExit();
}

void MinesweeperActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  const bool over = game.state == Minesweeper::State::Won || game.state == Minesweeper::State::Lost;
  const bool flag = !over && mappedInput.wasLongPressed(Button::Confirm, FLAG_HOLD_MS);
  const bool open = !flag && mappedInput.wasReleased(Button::Confirm);
  int dx = 0, dy = 0;
  if (mappedInput.wasReleased(Button::Left)) dx = -1;
  if (mappedInput.wasReleased(Button::Right)) dx = 1;
  if (mappedInput.wasReleased(Button::Up)) dy = -1;
  if (mappedInput.wasReleased(Button::Down)) dy = 1;
  if (!flag && !open && dx == 0 && dy == 0) return;
  {
    RenderLock lock;
    if (open && over) {
      game.reset();
    } else if (open) {
      game.reveal(cx, cy, hwRandom);
    } else if (flag) {
      game.toggleFlag(cx, cy);
    } else {
      cx = (cx + dx + Minesweeper::W) % Minesweeper::W;
      cy = (cy + dy + Minesweeper::H) % Minesweeper::H;
    }
    dirty = true;
  }
  requestUpdate();
}

void MinesweeperActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageW = renderer.getScreenWidth();
  const int pageH = renderer.getScreenHeight();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageW, metrics.headerHeight}, tr(STR_GAME_MINES), nullptr);
  constexpr int side = 24;
  int y = metrics.topPadding + metrics.headerHeight + 16;

  char line[64];
  snprintf(line, sizeof(line), "%s %d", tr(STR_GAME_MINES_LEFT), Minesweeper::MINES - game.flags());
  renderer.drawText(UI_12_FONT_ID, side, y, line, true, EpdFontFamily::BOLD);
  const char* status = game.state == Minesweeper::State::Won    ? tr(STR_GAME_WON)
                       : game.state == Minesweeper::State::Lost ? tr(STR_GAME_LOST)
                                                                : "";
  renderer.drawText(UI_12_FONT_ID, pageW - side - renderer.getTextWidth(UI_12_FONT_ID, status, EpdFontFamily::BOLD), y,
                    status, true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 16;

  // Closed cells grey, open cells white with their count, the cursor a thick
  // black frame inside its cell. A loss shows every mine as a black disc.
  constexpr int gap = 3;
  const int boardW = std::min(pageW - 2 * side, pageH - y - metrics.buttonHintsHeight - 40);
  const int cell = (boardW - (Minesweeper::W + 1) * gap) / Minesweeper::W;
  const int x0 = (pageW - boardW) / 2;
  renderer.fillRect(x0, y, boardW, boardW, true);
  const bool lost = game.state == Minesweeper::State::Lost;
  const int numH = renderer.getLineHeight(UI_12_FONT_ID);
  for (int r = 0; r < Minesweeper::H; ++r) {
    for (int c = 0; c < Minesweeper::W; ++c) {
      const int tx = x0 + gap + c * (cell + gap);
      const int ty = y + gap + r * (cell + gap);
      const uint8_t v = game.at(c, r);
      const bool shown = (v & Minesweeper::OPEN) || (lost && (v & Minesweeper::MINE));
      renderer.fillRect(tx, ty, cell, cell, false);
      if (!shown) renderer.fillRectDither(tx + 2, ty + 2, cell - 4, cell - 4, Color::LightGray);
      if (shown && (v & Minesweeper::MINE)) {
        const int d = cell / 2;
        renderer.fillRoundedRect(tx + (cell - d) / 2, ty + (cell - d) / 2, d, d, d / 2, Color::Black);
      } else if (shown && (v & Minesweeper::COUNT)) {
        char n[2] = {static_cast<char>('0' + (v & Minesweeper::COUNT)), '\0'};
        const int tw = renderer.getTextWidth(UI_12_FONT_ID, n, EpdFontFamily::BOLD);
        renderer.drawText(UI_12_FONT_ID, tx + (cell - tw) / 2, ty + (cell - numH) / 2, n, true, EpdFontFamily::BOLD);
      } else if (!shown && (v & Minesweeper::FLAG)) {
        const int px[3] = {tx + cell / 3, tx + cell * 3 / 4, tx + cell / 3};
        const int py[3] = {ty + cell / 5, ty + cell * 2 / 5, ty + cell * 3 / 5};
        renderer.fillPolygon(px, py, 3, true);
        renderer.fillRect(tx + cell / 3 - 2, ty + cell / 5, 3, cell * 3 / 5, true);
      }
      if (c == cx && r == cy) {
        renderer.drawRect(tx + 1, ty + 1, cell - 2, cell - 2, 4, true);  // inside: the gaps are black
      }
    }
  }
  y += boardW + 16;
  if (game.state == Minesweeper::State::Playing || game.state == Minesweeper::State::Fresh) {
    renderer.drawCenteredText(UI_10_FONT_ID, y, tr(STR_GAME_MINES_HELP));
  }

  const bool over = game.state == Minesweeper::State::Won || lost;
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), over ? tr(STR_GAME_NEW) : tr(STR_GAME_OPEN), "<", ">");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (++fastRefreshes >= FAST_REFRESHES_PER_HALF || over) {
    fastRefreshes = 0;
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  } else {
    renderer.displayBuffer();
  }
}
