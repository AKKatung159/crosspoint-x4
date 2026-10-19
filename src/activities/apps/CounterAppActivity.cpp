#include "CounterAppActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <cstdio>

#include "MappedInputManager.h"
#include "apps/AppSave.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char APP_ID[] = "counter";
constexpr uint16_t SAVE_VERSION = 1;
}  // namespace

void CounterAppActivity::onEnter() {
  Activity::onEnter();
  if (!app_save::load(APP_ID, SAVE_VERSION, &count, sizeof(count))) count = 0;
  LOG_INF("COUNTER", "loaded %ld", static_cast<long>(count));
  requestUpdate();
}

void CounterAppActivity::onExit() {
  if (dirty) {
    const bool ok = app_save::save(APP_ID, SAVE_VERSION, &count, sizeof(count), sizeof(count));
    LOG_INF("COUNTER", "saved %ld: %s", static_cast<long>(count), ok ? "ok" : "FAILED");
  }
  Activity::onExit();
}

void CounterAppActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  int step = 0;
  if (mappedInput.wasReleased(Button::Up) || mappedInput.wasReleased(Button::Confirm)) step = 1;
  if (mappedInput.wasReleased(Button::Down)) step = -1;
  if (step == 0) return;
  {
    RenderLock lock;
    count += step;
    dirty = true;
  }
  requestUpdate();
}

void CounterAppActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageW = renderer.getScreenWidth();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageW, metrics.headerHeight}, tr(STR_APP_COUNTER), nullptr);
  char value[16];
  snprintf(value, sizeof(value), "%ld", static_cast<long>(count));
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, renderer.getScreenHeight() / 2 - 20, value, true, EpdFontFamily::BOLD);
  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "+", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
