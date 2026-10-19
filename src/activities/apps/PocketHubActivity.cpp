#include "PocketHubActivity.h"

#include <Memory.h>

#include "activities/home/ReadingStatsActivity.h"
#include "components/UITheme.h"
#ifdef PERSONAL_SERIAL_INPUT
#include "CounterAppActivity.h"
#endif

namespace fui = freeink::ui;

namespace {

template <typename T>
std::unique_ptr<Activity> make(GfxRenderer& renderer, MappedInputManager& input) {
  return makeUniqueNoThrow<T>(renderer, input);
}

struct HubApp {
  StrId name;
  std::unique_ptr<Activity> (*create)(GfxRenderer&, MappedInputManager&);
};

constexpr HubApp APPS[] = {
    {StrId::STR_READING_STATS, &make<ReadingStatsActivity>},
#ifdef PERSONAL_SERIAL_INPUT
    // Test builds only: exercises app saves across Back, sleep and wake.
    {StrId::STR_APP_COUNTER, &make<CounterAppActivity>},
#endif
};
constexpr int APP_COUNT = sizeof(APPS) / sizeof(APPS[0]);

}  // namespace

int PocketHubActivity::listCount() const { return APP_COUNT < MAX_APPS ? APP_COUNT : MAX_APPS; }

void PocketHubActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  nav.selected = index;
  auto activity = APPS[index].create(renderer, mappedInput);
  if (!activity) {
    LOG_ERR("HUB", "OOM: app %d", index);
    return;
  }
  activityManager.pushActivity(std::move(activity));
}

void PocketHubActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                                      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                                      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)),
                                      static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  for (int i = 0; i < listCount(); ++i) {
    rows[i].label = I18N.get(APPS[i].name);
    rows[i].actionValue = static_cast<int16_t>(i);
  }
  fui::ListProps props;
  props.items = rows;
  props.count = listCount();
  props.action = ACTION_ROW;
  props.inputMask = fui::InputTouch;
  syncListViewport(screen, props);
  screen.list(props);
}
