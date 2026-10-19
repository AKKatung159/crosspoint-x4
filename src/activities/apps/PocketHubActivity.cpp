#include "PocketHubActivity.h"

#include <HalStorage.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>

#include "FlashcardsActivity.h"
#include "Game2048Activity.h"
#include "MinesweeperActivity.h"
#include "NonogramActivity.h"
#include "TarotActivity.h"
#include "WasmAppActivity.h"
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
    {StrId::STR_FLASHCARDS, &make<FlashcardsActivity>},
    {StrId::STR_TAROT, &make<TarotActivity>},
    {StrId::STR_GAME_2048, &make<Game2048Activity>},
    {StrId::STR_GAME_MINES, &make<MinesweeperActivity>},
    {StrId::STR_GAME_NONOGRAM, &make<NonogramActivity>},
    {StrId::STR_READING_STATS, &make<ReadingStatsActivity>},
#ifdef PERSONAL_SERIAL_INPUT
    // Test builds only: exercises app saves across Back, sleep and wake.
    {StrId::STR_APP_COUNTER, &make<CounterAppActivity>},
#endif
};
constexpr int APP_COUNT = sizeof(APPS) / sizeof(APPS[0]);

}  // namespace

void PocketHubActivity::onEnter() {
  sdkApps.clear();
  sdkApps.reserve(MAX_SDK_APPS);
  auto dir = Storage.open("/apps");
  if (dir && dir.isDirectory()) {
    char folder[40];
    for (auto f = dir.openNextFile(); f && static_cast<int>(sdkApps.size()) < MAX_SDK_APPS; f = dir.openNextFile()) {
      if (!f.isDirectory()) continue;
      f.getName(folder, sizeof(folder));
      AppManifest m;
      if (folder[0] != '.' && m.load(folder)) sdkApps.push_back(m);
    }
  }
  std::sort(sdkApps.begin(), sdkApps.end(),
            [](const AppManifest& a, const AppManifest& b) { return strcmp(a.nameEn, b.nameEn) < 0; });
  LOG_INF("HUB", "%u SDK apps", static_cast<unsigned>(sdkApps.size()));
  UiListActivity::onEnter();
}

int PocketHubActivity::listCount() const {
  return std::min(APP_COUNT + static_cast<int>(sdkApps.size()), MAX_APPS);
}

void PocketHubActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  nav.selected = index;
  auto activity = index < APP_COUNT
                      ? APPS[index].create(renderer, mappedInput)
                      : makeUniqueNoThrow<WasmAppActivity>(renderer, mappedInput, sdkApps[index - APP_COUNT]);
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
    rows[i].label = i < APP_COUNT ? I18N.get(APPS[i].name) : sdkApps[i - APP_COUNT].name();
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
