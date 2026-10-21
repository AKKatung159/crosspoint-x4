#include "TarotActivity.h"

#include <Memory.h>

#include "TarotReadingActivity.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

void TarotActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  nav.selected = index;
  auto reading = makeUniqueNoThrow<TarotReadingActivity>(renderer, mappedInput, index == 0 ? 1 : 3);
  if (!reading) {
    LOG_ERR("TAROT", "OOM: reading activity");
    return;
  }
  activityManager.pushActivity(std::move(reading));
}

void TarotActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                                      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                                      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)),
                                      static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  rows[0].label = tr(STR_TAROT_DAILY);
  rows[1].label = tr(STR_TAROT_THREE);
  for (int i = 0; i < 2; ++i) rows[i].actionValue = static_cast<int16_t>(i);
  fui::ListProps props;
  props.items = rows;
  props.count = listCount();
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}
