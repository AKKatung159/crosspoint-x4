#include "FlashcardStudyActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>

#include "MappedInputManager.h"
#include "apps/AppSave.h"
#include "apps/AppText.h"
#include "apps/TsvCard.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {
constexpr char APP_ID[] = "flashcards";
constexpr char INDEX_DIR[] = "/.crosspoint/apps/flashcards";
constexpr uint16_t SAVE_VERSION = 1;
// A half refresh after this many fast ones clears e-ink ghosting.
constexpr int FAST_REFRESHES_PER_HALF = 10;
}  // namespace

void FlashcardStudyActivity::onEnter() {
  Activity::onEnter();
  deckOk = deck.open(DECK_DIR, deckName.c_str(), INDEX_DIR) && deck.count() > 0;
  if (deckOk) {
    Progress p;
    const uint32_t h = TsvFile::nameHash(deckName.c_str());
    if (app_save::load(APP_ID, SAVE_VERSION, &p, sizeof(p))) {
      for (int i = 0; i < RECENT; ++i) {
        if (p.hash[i] == h && p.pos[i] < deck.count()) index = p.pos[i];
      }
    }
    loadCard();
  }
  LOG_INF("FLASH", "deck %s: %lu cards, at %lu", deckName.c_str(), static_cast<unsigned long>(deck.count()),
          static_cast<unsigned long>(index));
  requestUpdate();
}

void FlashcardStudyActivity::onExit() {
  if (moved) {
    Progress p;
    app_save::load(APP_ID, SAVE_VERSION, &p, sizeof(p));
    const uint32_t h = TsvFile::nameHash(deckName.c_str());
    int slot = -1;
    for (int i = 0; i < RECENT; ++i) {
      if (p.hash[i] == h) slot = i;
    }
    if (slot < 0) {
      slot = static_cast<int>(p.next % RECENT);
      p.next = (p.next + 1) % RECENT;
    }
    p.hash[slot] = h;
    p.pos[slot] = index;
    const bool ok = app_save::save(APP_ID, SAVE_VERSION, &p, sizeof(p), sizeof(p));
    LOG_INF("FLASH", "saved %s at %lu: %s", deckName.c_str(), static_cast<unsigned long>(index), ok ? "ok" : "FAILED");
  }
  Activity::onExit();
}

void FlashcardStudyActivity::loadCard() {
  if (!deck.read(index, line) || !tsv_card::parse(line, &front, &back)) {
    LOG_ERR("FLASH", "Cannot read card %lu", static_cast<unsigned long>(index));
    front = back = "";
  }
}

void FlashcardStudyActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  if (!deckOk) return;
  int step = 0;
  if (mappedInput.wasReleased(Button::Down) || mappedInput.wasReleased(Button::Right)) step = 1;
  if (mappedInput.wasReleased(Button::Up) || mappedInput.wasReleased(Button::Left)) step = -1;
  const bool flip = mappedInput.wasReleased(Button::Confirm);
  if (!flip && step == 0) return;
  {
    RenderLock lock;
    if (flip) {
      flipped = !flipped;
    } else {
      const uint32_t n = deck.count();
      index = (index + n + step) % n;  // wraps at both ends
      flipped = false;
      moved = true;
      loadCard();
    }
  }
  requestUpdate();
}

void FlashcardStudyActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageW = renderer.getScreenWidth();
  const int pageH = renderer.getScreenHeight();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageW, metrics.headerHeight}, deckName.c_str(), nullptr);
  constexpr int side = 24;
  const int innerW = pageW - 2 * side;
  const int top = metrics.topPadding + metrics.headerHeight + 12;
  const int bottom = pageH - metrics.buttonHintsHeight - 12;

  if (!deckOk) {
    renderer.drawCenteredText(UI_12_FONT_ID, pageH / 2, tr(STR_FLASH_EMPTY_DECK));
  } else {
    char pos[32];
    snprintf(pos, sizeof(pos), "%lu / %lu", static_cast<unsigned long>(index + 1),
             static_cast<unsigned long>(deck.count()));
    renderer.drawText(UI_10_FONT_ID, pageW - side - renderer.getTextWidth(UI_10_FONT_ID, pos), top, pos);

    // Front in the upper half, back under a grey band in the lower half.
    const int mid = (top + bottom) / 2;
    const int frontLineH = renderer.getLineHeight(NOTOSANS_18_FONT_ID);
    const auto frontLines = app_text::wrap(renderer, NOTOSANS_18_FONT_ID, front, innerW,
                                           std::max(1, (mid - top - 40) / frontLineH), EpdFontFamily::BOLD);
    int y = top + 30 + (mid - top - 30 - static_cast<int>(frontLines.size()) * frontLineH) / 2;
    for (const auto& l : frontLines) {
      renderer.drawCenteredText(NOTOSANS_18_FONT_ID, y, l.c_str(), true, EpdFontFamily::BOLD);
      y += frontLineH;
    }
    renderer.fillRoundedRect(side, mid, innerW, 6, 3, Color::LightGray);
    if (flipped) {
      const int backLineH = renderer.getLineHeight(NOTOSANS_16_FONT_ID);
      y = mid + 24;
      for (const auto& l :
           app_text::wrap(renderer, NOTOSANS_16_FONT_ID, back, innerW, std::max(1, (bottom - y) / backLineH))) {
        renderer.drawCenteredText(NOTOSANS_16_FONT_ID, y, l.c_str());
        y += backLineH;
      }
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_FLASH_FLIP), "<", ">");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (++fastRefreshes >= FAST_REFRESHES_PER_HALF) {
    fastRefreshes = 0;
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  } else {
    renderer.displayBuffer();
  }
}
