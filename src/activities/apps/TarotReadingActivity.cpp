#include "TarotReadingActivity.h"

#include <Arduino.h>
#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "apps/AppText.h"
#include "apps/TsvCard.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char TAROT_DIR[] = "/apps/tarot";
constexpr char INDEX_DIR[] = "/.crosspoint/apps/tarot";
constexpr int SIDE = 24;

bool thai() { return I18N.getLanguage() == Language::TH; }
}  // namespace

void TarotReadingActivity::onEnter() {
  Activity::onEnter();
  deckOk = deck.open(TAROT_DIR, "deck.tsv", INDEX_DIR) && deck.count() >= static_cast<uint32_t>(count);
  if (deckOk) {
    // Distinct cards from the hardware RNG; a repeat takes the next card, so
    // this ends even with a stuck RNG.
    for (int i = 0; i < count; ++i) {
      drawn[i] = esp_random() % deck.count();
      while (std::find(drawn, drawn + i, drawn[i]) != drawn + i) drawn[i] = (drawn[i] + 1) % deck.count();
      reversed[i] = esp_random() & 1U;
    }
    loadCard();
  }
  LOG_INF("TAROT", "spread of %d from %lu cards, first %lu%s", count, static_cast<unsigned long>(deck.count()),
          static_cast<unsigned long>(drawn[0]), reversed[0] ? " reversed" : "");
  requestUpdate();
}

void TarotReadingActivity::loadCard() {
  for (auto& c : cols) c = row + sizeof(row) - 1;  // "" for missing columns
  if (!deck.read(drawn[current], row) || tsv_card::split(row, cols, COLUMNS) < UP_EN) {
    LOG_ERR("TAROT", "Bad deck row %lu", static_cast<unsigned long>(drawn[current]));
  }
}

void TarotReadingActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  if (!deckOk) return;
  int step = 0;
  if (mappedInput.wasReleased(Button::Right) || mappedInput.wasReleased(Button::Down)) step = 1;
  if (mappedInput.wasReleased(Button::Left) || mappedInput.wasReleased(Button::Up)) step = -1;
  const bool flip = mappedInput.wasReleased(Button::Confirm);
  if (!flip && (step == 0 || count == 1)) return;
  {
    RenderLock lock;
    if (flip) {
      showMeaning = !showMeaning;
    } else {
      current = (current + count + step) % count;
      showMeaning = false;
      loadCard();
    }
  }
  requestUpdate();
}

void TarotReadingActivity::renderImage(const int top, const int bottom) const {
  const int pageW = renderer.getScreenWidth();
  const int nameH = renderer.getLineHeight(NOTOSANS_16_FONT_ID) + renderer.getLineHeight(UI_10_FONT_ID) + 12;
  const int boxH = bottom - top - nameH;
  char path[96];
  snprintf(path, sizeof(path), "%s/cards/%s.bmp", TAROT_DIR, cols[FILE]);
  bool shown = false;
  HalFile file;
  if (Storage.exists(path) && Storage.openFileForRead("TAROT", path, file)) {
    Bitmap bitmap(file);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) {
      const int w = std::min(bitmap.getWidth(), pageW - 2 * SIDE);
      shown = renderer.drawBitmap(bitmap, (pageW - w) / 2, top, pageW - 2 * SIDE, boxH);
    }
  }
  if (!shown) {
    // No picture copied yet: a plain card back keeps the screen readable.
    const int w = boxH * 58 / 100;
    renderer.fillRoundedRect((pageW - w) / 2, top, w, boxH, 16, Color::LightGray);
  }
  const char* name = thai() ? cols[NAME_TH] : cols[NAME_EN];
  int y = bottom - nameH + 6;
  renderer.drawCenteredText(NOTOSANS_16_FONT_ID, y, name, true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(NOTOSANS_16_FONT_ID);
  renderer.drawCenteredText(UI_10_FONT_ID, y, reversed[current] ? tr(STR_TAROT_REVERSED) : tr(STR_TAROT_UPRIGHT));
}

void TarotReadingActivity::renderMeaning(const int top, const int bottom) const {
  const int innerW = renderer.getScreenWidth() - 2 * SIDE;
  const bool th = thai();
  const bool rev = reversed[current];
  int y = top + 8;
  for (const auto& l : app_text::wrap(renderer, NOTOSANS_18_FONT_ID, th ? cols[NAME_TH] : cols[NAME_EN], innerW, 2,
                                      EpdFontFamily::BOLD)) {
    renderer.drawText(NOTOSANS_18_FONT_ID, SIDE, y, l.c_str(), true, EpdFontFamily::BOLD);
    y += renderer.getLineHeight(NOTOSANS_18_FONT_ID);
  }
  renderer.drawText(UI_12_FONT_ID, SIDE, y + 4, rev ? tr(STR_TAROT_REVERSED) : tr(STR_TAROT_UPRIGHT));
  y += renderer.getLineHeight(UI_12_FONT_ID) + 16;
  renderer.fillRoundedRect(SIDE, y, innerW, 6, 3, Color::LightGray);
  y += 24;

  // The UI language first, the other one under it, smaller.
  const char* primary = rev ? (th ? cols[REV_TH] : cols[REV_EN]) : (th ? cols[UP_TH] : cols[UP_EN]);
  const char* secondary = rev ? (th ? cols[REV_EN] : cols[REV_TH]) : (th ? cols[UP_EN] : cols[UP_TH]);
  const int primaryH = renderer.getLineHeight(NOTOSANS_16_FONT_ID);
  for (const auto& l : app_text::wrap(renderer, NOTOSANS_16_FONT_ID, primary, innerW, 6)) {
    renderer.drawText(NOTOSANS_16_FONT_ID, SIDE, y, l.c_str());
    y += primaryH;
  }
  y += 16;
  const int secondaryH = renderer.getLineHeight(UI_12_FONT_ID);
  for (const auto& l :
       app_text::wrap(renderer, UI_12_FONT_ID, secondary, innerW, std::max(1, (bottom - y) / secondaryH))) {
    renderer.drawText(UI_12_FONT_ID, SIDE, y, l.c_str());
    y += secondaryH;
  }
}

void TarotReadingActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageW = renderer.getScreenWidth();
  const int pageH = renderer.getScreenHeight();
  static constexpr StrId POSITIONS[MAX_CARDS] = {StrId::STR_TAROT_PAST, StrId::STR_TAROT_PRESENT,
                                                 StrId::STR_TAROT_FUTURE};
  const char* title = count == 1 ? tr(STR_TAROT_DAILY) : I18N.get(POSITIONS[current]);
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageW, metrics.headerHeight}, title, nullptr);
  const int top = metrics.topPadding + metrics.headerHeight + 12;
  const int bottom = pageH - metrics.buttonHintsHeight - 8;

  if (!deckOk) {
    int y = (top + bottom) / 2 - 60;
    for (const auto& l : app_text::wrap(renderer, UI_12_FONT_ID, tr(STR_TAROT_MISSING), pageW - 2 * SIDE, 6)) {
      renderer.drawCenteredText(UI_12_FONT_ID, y, l.c_str());
      y += renderer.getLineHeight(UI_12_FONT_ID);
    }
  } else {
    if (count > 1) {
      char pos[16];
      snprintf(pos, sizeof(pos), "%d / %d", current + 1, count);
      renderer.drawText(UI_10_FONT_ID, pageW - SIDE - renderer.getTextWidth(UI_10_FONT_ID, pos),
                        metrics.topPadding + metrics.headerHeight + 4, pos);
    }
    showMeaning ? renderMeaning(top + 16, bottom) : renderImage(top + 16, bottom);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), showMeaning ? tr(STR_TAROT_CARD) : tr(STR_TAROT_MEANING),
                                            count > 1 ? "<" : "", count > 1 ? ">" : "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  // A picture leaves more ghosting than text; clear it with a half refresh.
  renderer.displayBuffer(showMeaning ? HalDisplay::FAST_REFRESH : HalDisplay::HALF_REFRESH);
}
