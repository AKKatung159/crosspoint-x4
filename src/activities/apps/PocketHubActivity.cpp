#include "PocketHubActivity.h"

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <Memory.h>

#include <algorithm>
#include <cstring>

#include "FlashcardsActivity.h"
#include "Game2048Activity.h"
#include "MappedInputManager.h"
#include "MinesweeperActivity.h"
#include "NonogramActivity.h"
#include "TarotActivity.h"
#include "WasmAppActivity.h"
#include "activities/home/ReadingStatsActivity.h"
#include "apps/AppText.h"
#include "components/UITheme.h"
#include "components/icons/personal/homeIcons.h"
#include "fontIds.h"
#ifdef PERSONAL_SERIAL_INPUT
#include "CounterAppActivity.h"
#endif

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
  selected = std::min(selected, std::max(0, count() - 1));
  Activity::onEnter();
  requestUpdate();
}

int PocketHubActivity::count() const { return std::min(builtIns + static_cast<int>(sdkApps.size()), MAX_APPS); }

const char* PocketHubActivity::name(const int index) const {
  return index < builtIns ? I18N.get(APPS[builtIn[index]].name) : sdkApps[index - builtIns].name();
}

void PocketHubActivity::open(const int index) {
  if (index < 0 || index >= count()) return;
  auto activity = index < builtIns
                      ? APPS[builtIn[index]].create(renderer, mappedInput)
                      : makeUniqueNoThrow<WasmAppActivity>(renderer, mappedInput, sdkApps[index - builtIns]);
  if (!activity) {
    LOG_ERR("HUB", "OOM: app %d", index);
    return;
  }
  activityManager.pushActivity(std::move(activity));
}

// Left/Right step one cell (wrapping); Up/Down one row, landing on the last
// cell when the row below is short and wrapping between the first and last rows.
void PocketHubActivity::move(const int delta) {
  const int n = count();
  if (n == 0) return;
  if (delta == 1 || delta == -1) {
    selected = (selected + delta + n) % n;
    return;
  }
  const int lastRow = (n - 1) / COLUMNS, row = selected / COLUMNS, col = selected % COLUMNS;
  int target = delta > 0 ? (row == lastRow ? 0 : row + 1) : (row == 0 ? lastRow : row - 1);
  selected = std::min(target * COLUMNS + col, n - 1);
}

void PocketHubActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    activityManager.goHome();  // so Home reselects the Apps tile
    return;
  }
  if (mappedInput.wasReleased(Button::Confirm)) {
    open(selected);
    return;
  }
  int delta = 0;
  if (mappedInput.wasReleased(Button::Right)) delta = 1;
  if (mappedInput.wasReleased(Button::Left)) delta = -1;
  if (mappedInput.wasReleased(Button::Down)) delta = COLUMNS;
  if (mappedInput.wasReleased(Button::Up)) delta = -COLUMNS;
  if (delta == 0) return;
  {
    RenderLock lock;
    move(delta);
  }
  requestUpdate();
}

// One cell: the icon doubled to 64 px (or the home Apps icon for apps without
// one) over a centred name of up to two lines. The selected cell is a black
// rounded card with the icon and name knocked out in white.
void PocketHubActivity::drawCell(const int index, const int x, const int y, const int w, const int h) const {
  const bool on = index == selected;
  if (on) renderer.fillRoundedRect(x, y, w, h, 18, Color::Black);
  constexpr int ICON = 64;
  const int ix = x + (w - ICON) / 2, iy = y + 14;
  const AppManifest* m = index >= builtIns ? &sdkApps[index - builtIns] : nullptr;
  if (m && m->hasIcon) {
    constexpr int N = AppManifest::ICON_SIZE, SCALE = ICON / N;
    for (int r = 0; r < N; ++r) {
      for (int c = 0; c < N; ++c) {
        if ((m->icon[r * (N / 8) + c / 8] >> (7 - c % 8)) & 1) {
          renderer.fillRect(ix + c * SCALE, iy + r * SCALE, SCALE, SCALE, !on);
        }
      }
    }
  } else {
    // HomeApps44: 1bpp, MSB first, 0 = ink, rotated as GfxRenderer::drawIcon reads it.
    constexpr int S = 44;
    const int ox = ix + (ICON - S) / 2, oy = iy + (ICON - S) / 2;
    for (int r = 0; r < S; ++r) {
      for (int c = 0; c < S; ++c) {
        if (((HomeApps44[r * ((S + 7) / 8) + c / 8] >> (7 - c % 8)) & 1) == 0) {
          renderer.drawPixel(ox + (S - 1 - r), oy + c, !on);
        }
      }
    }
  }
  const int lh = renderer.getLineHeight(UI_12_FONT_ID);
  int ty = iy + ICON + 10;
  for (const auto& line : app_text::wrap(renderer, UI_12_FONT_ID, name(index), w - 12, 2)) {
    const int tw = renderer.getTextWidth(UI_12_FONT_ID, line.c_str());
    renderer.drawText(UI_12_FONT_ID, x + (w - tw) / 2, ty, line.c_str(), !on);
    ty += lh;
  }
}

void PocketHubActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageW = renderer.getScreenWidth(), pageH = renderer.getScreenHeight();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageW, metrics.headerHeight}, tr(STR_POCKET_HUB), nullptr);

  constexpr int SIDE = 16, GAP = 10;
  const int top = metrics.topPadding + metrics.headerHeight + 14;
  const int bottom = pageH - metrics.buttonHintsHeight - 8;
  const int cellW = (pageW - 2 * SIDE - (COLUMNS - 1) * GAP) / COLUMNS;
  const int cellH = 14 + 64 + 10 + 2 * renderer.getLineHeight(UI_12_FONT_ID) + 12;
  const int visibleRows = std::max(1, (bottom - top + GAP) / (cellH + GAP));
  const int row = selected / COLUMNS;
  if (row < firstRow) firstRow = row;
  if (row >= firstRow + visibleRows) firstRow = row - visibleRows + 1;

  const int n = count();
  for (int i = firstRow * COLUMNS; i < n && i < (firstRow + visibleRows) * COLUMNS; ++i) {
    const int r = i / COLUMNS - firstRow, c = i % COLUMNS;
    drawCell(i, SIDE + c * (cellW + GAP), top + r * (cellH + GAP), cellW, cellH);
  }
  // More rows above or below: a small triangle in the margin.
  const int cx = pageW / 2;
  if (firstRow > 0) {
    const int xs[3] = {cx - 8, cx + 8, cx}, ys[3] = {top - 4, top - 4, top - 12};
    renderer.fillPolygon(xs, ys, 3, true);
  }
  if ((firstRow + visibleRows) * COLUMNS < n) {
    const int y = top + visibleRows * (cellH + GAP) - GAP + 4;
    const int xs[3] = {cx - 8, cx + 8, cx}, ys[3] = {y, y, y + 8};
    renderer.fillPolygon(xs, ys, 3, true);
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), tr(STR_OPEN), "<", ">");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  if (++fastRefreshes >= 15) {
    fastRefreshes = 0;
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  } else {
    renderer.displayBuffer();
  }
}
