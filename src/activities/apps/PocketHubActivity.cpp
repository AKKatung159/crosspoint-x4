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
  const char* sdkId;  // an installed SDK app with this id replaces the built-in one
};

constexpr HubApp APPS[] = {
    {StrId::STR_FLASHCARDS, &make<FlashcardsActivity>, "flashcards"},
    {StrId::STR_TAROT, &make<TarotActivity>, "tarot"},
    {StrId::STR_GAME_2048, &make<Game2048Activity>, "2048"},
    {StrId::STR_GAME_MINES, &make<MinesweeperActivity>, "minesweeper"},
    {StrId::STR_GAME_NONOGRAM, &make<NonogramActivity>, "nonogram"},
    {StrId::STR_READING_STATS, &make<ReadingStatsActivity>, "stats"},
#ifdef PERSONAL_SERIAL_INPUT
    // Test builds only: exercises app saves across Back, sleep and wake.
    {StrId::STR_APP_COUNTER, &make<CounterAppActivity>, nullptr},
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
  builtIns = 0;
  for (int i = 0; i < APP_COUNT; ++i) {
    const bool replaced = APPS[i].sdkId && std::any_of(sdkApps.begin(), sdkApps.end(), [i](const AppManifest& m) {
                            return strcmp(m.id, APPS[i].sdkId) == 0;
                          });
    if (!replaced) builtIn[builtIns++] = static_cast<uint8_t>(i);
  }
  LOG_INF("HUB", "%u SDK apps, %d built-in", static_cast<unsigned>(sdkApps.size()), builtIns);
  UiListActivity::onEnter();
}

int PocketHubActivity::listCount() const { return std::min(builtIns + static_cast<int>(sdkApps.size()), MAX_APPS); }

void PocketHubActivity::activateIndex(const int index) {
  if (index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  nav.selected = index;
  auto activity = index < builtIns
                      ? APPS[builtIn[index]].create(renderer, mappedInput)
                      : makeUniqueNoThrow<WasmAppActivity>(renderer, mappedInput, sdkApps[index - builtIns]);
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
    rows[i].label = i < builtIns ? I18N.get(APPS[builtIn[i]].name) : sdkApps[i - builtIns].name();
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
