#include "Game2048Activity.h"

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
constexpr char APP_ID[] = "2048";
constexpr uint16_t SAVE_VERSION = 1;
constexpr int FAST_REFRESHES_PER_HALF = 15;

struct Saved {
  uint8_t cells[Game2048::N * Game2048::N];
  uint32_t score;
  uint32_t best;
};
}  // namespace

void Game2048Activity::newGame() {
  game.reset();
  game.spawn(esp_random(), esp_random());
  game.spawn(esp_random(), esp_random());
  dirty = true;
}

void Game2048Activity::onEnter() {
  Activity::onEnter();
  Saved s;
  const bool loaded = app_save::load(APP_ID, SAVE_VERSION, &s, sizeof(s));
  if (loaded) {
    memcpy(game.cells, s.cells, sizeof(s.cells));
    game.score = s.score;
    game.best = s.best;
  }
  if (!loaded || !game.canMove()) {
    const uint32_t best = game.best;
    newGame();
    game.best = best;
  }
  requestUpdate();
}

void Game2048Activity::onExit() {
  if (dirty) {
    Saved s;
    memcpy(s.cells, game.cells, sizeof(s.cells));
    s.score = game.score;
    s.best = game.best;
    const bool ok = app_save::save(APP_ID, SAVE_VERSION, &s, sizeof(s), sizeof(s));
    LOG_INF("2048", "saved score %lu: %s", static_cast<unsigned long>(game.score), ok ? "ok" : "FAILED");
  }
  Activity::onExit();
}

void Game2048Activity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  if (!game.canMove()) {
    if (mappedInput.wasReleased(Button::Confirm)) {
      {
        RenderLock lock;
        const uint32_t best = game.best;
        newGame();
        game.best = best;
      }
      requestUpdate();
    }
    return;
  }
  Game2048::Dir dir;
  if (mappedInput.wasReleased(Button::Left)) {
    dir = Game2048::LEFT;
  } else if (mappedInput.wasReleased(Button::Right)) {
    dir = Game2048::RIGHT;
  } else if (mappedInput.wasReleased(Button::Up)) {
    dir = Game2048::UP;
  } else if (mappedInput.wasReleased(Button::Down)) {
    dir = Game2048::DOWN;
  } else {
    return;
  }
  {
    RenderLock lock;
    if (!game.move(dir)) return;
    game.spawn(esp_random(), esp_random());
    dirty = true;
  }
  LOG_DBG("2048", "move %d, score %lu", dir, static_cast<unsigned long>(game.score));
  requestUpdate();
}

void Game2048Activity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageW = renderer.getScreenWidth();
  const int pageH = renderer.getScreenHeight();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageW, metrics.headerHeight}, tr(STR_GAME_2048), nullptr);
  constexpr int side = 24;
  int y = metrics.topPadding + metrics.headerHeight + 16;

  char line[64];
  snprintf(line, sizeof(line), "%s %lu", tr(STR_GAME_SCORE), static_cast<unsigned long>(game.score));
  renderer.drawText(UI_12_FONT_ID, side, y, line, true, EpdFontFamily::BOLD);
  snprintf(line, sizeof(line), "%s %lu", tr(STR_GAME_BEST), static_cast<unsigned long>(game.best));
  renderer.drawText(UI_12_FONT_ID, pageW - side - renderer.getTextWidth(UI_12_FONT_ID, line), y, line);
  y += renderer.getLineHeight(UI_12_FONT_ID) + 16;

  // Board dark grey and empty cells left bare; 2 and 4 white, 8 to 32 light
  // grey, 64 and up solid black with white numbers.
  const int boardW = std::min(pageW - 2 * side, pageH - y - metrics.buttonHintsHeight - 80);
  constexpr int gap = 10;
  const int tile = (boardW - (Game2048::N + 1) * gap) / Game2048::N;
  const int x0 = (pageW - boardW) / 2;
  renderer.fillRoundedRect(x0, y, boardW, boardW, 14, Color::DarkGray);
  const int lineH = renderer.getLineHeight(NOTOSANS_18_FONT_ID);
  for (int r = 0; r < Game2048::N; ++r) {
    for (int c = 0; c < Game2048::N; ++c) {
      const int tx = x0 + gap + c * (tile + gap);
      const int ty = y + gap + r * (tile + gap);
      const uint8_t v = game.at(r, c);
      if (v == 0) continue;
      const bool dark = v >= 6;  // 64 and up
      renderer.fillRoundedRect(tx, ty, tile, tile, 10, dark ? Color::Black : v >= 3 ? Color::LightGray : Color::White);
      char num[8];
      snprintf(num, sizeof(num), "%lu", 1UL << v);
      const int tw = renderer.getTextWidth(NOTOSANS_18_FONT_ID, num, EpdFontFamily::BOLD);
      renderer.drawText(NOTOSANS_18_FONT_ID, tx + (tile - tw) / 2, ty + (tile - lineH) / 2, num, !dark,
                        EpdFontFamily::BOLD);
    }
  }
  y += boardW + 20;
  if (!game.canMove()) {
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_GAME_OVER), true, EpdFontFamily::BOLD);
  } else if (game.won()) {
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_GAME_2048_WON), true, EpdFontFamily::BOLD);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), game.canMove() ? "" : tr(STR_GAME_NEW), "<", ">");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (++fastRefreshes >= FAST_REFRESHES_PER_HALF) {
    fastRefreshes = 0;
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  } else {
    renderer.displayBuffer();
  }
}
