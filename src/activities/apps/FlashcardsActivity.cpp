#include "FlashcardsActivity.h"

#include <HalStorage.h>
#include <Memory.h>

#include <algorithm>

#include "FlashcardStudyActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

void FlashcardsActivity::onEnter() {
  decks.clear();
  decks.reserve(MAX_DECKS);
  auto dir = Storage.open(DECK_DIR);
  if (dir && dir.isDirectory()) {
    char name[128];
    for (auto f = dir.openNextFile(); f && static_cast<int>(decks.size()) < MAX_DECKS; f = dir.openNextFile()) {
      f.getName(name, sizeof(name));
      const size_t len = strlen(name);
      if (!f.isDirectory() && name[0] != '.' && len > 4 && strcasecmp(name + len - 4, ".tsv") == 0) {
        decks.emplace_back(name);
      }
    }
  }
  std::sort(decks.begin(), decks.end());
  UiListActivity::onEnter();
}

void FlashcardsActivity::activateIndex(const int index) {
  if (decks.empty() || index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  nav.selected = index;
  auto study = makeUniqueNoThrow<FlashcardStudyActivity>(renderer, mappedInput, decks[index]);
  if (!study) {
    LOG_ERR("FLASH", "OOM: study activity");
    return;
  }
  activityManager.pushActivity(std::move(study));
}

void FlashcardsActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                                      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                                      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)),
                                      static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  if (decks.empty()) {
    rows[0].label = tr(STR_FLASH_NO_DECKS);
    rows[0].enabled = false;
  }
  for (int i = 0; i < static_cast<int>(decks.size()); ++i) {
    rows[i].label = decks[i].c_str();
    rows[i].actionValue = static_cast<int16_t>(i);
  }
  fui::ListProps props;
  props.items = rows;
  props.count = listCount();
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  props.labelText = screen.theme().bodyText;
  props.labelText.maxLines = 3;
  syncListViewport(screen, props);
  screen.list(props);
}
