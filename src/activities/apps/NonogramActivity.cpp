#include "NonogramActivity.h"

#include <Memory.h>

#include <cstdio>

#include "NonogramPlayActivity.h"
#include "apps/AppSave.h"
#include "apps/TsvCard.h"
#include "components/UITheme.h"

namespace fui = freeink::ui;

namespace {
constexpr char APP_ID[] = "nonogram";
constexpr uint16_t SAVE_VERSION = 1;
}  // namespace

bool NonogramActivity::loadProgress(Progress& p) { return app_save::load(APP_ID, SAVE_VERSION, &p, sizeof(p)); }

bool NonogramActivity::saveProgress(const Progress& p) {
  return app_save::save(APP_ID, SAVE_VERSION, &p, sizeof(p), sizeof(p));
}

bool NonogramActivity::openPuzzles(TsvFile& file) {
  return file.open("/apps/nonogram", "puzzles.tsv", "/.crosspoint/apps/nonogram");
}

void NonogramActivity::onEnter() {
  names.clear();
  loadProgress(progress);
  TsvFile file;
  auto row = makeUniqueNoThrow<char[]>(TsvFile::MAX_LINE + 1);
  if (row && openPuzzles(file)) {
    const int n = std::min<int>(static_cast<int>(file.count()), MAX_PUZZLES);
    names.reserve(n);
    for (int i = 0; i < n; ++i) {
      char* cols[2];
      names.emplace_back(file.read(i, row.get()) && tsv_card::split(row.get(), cols, 2) == 2 ? cols[0] : "?");
    }
  }
  UiListActivity::onEnter();
}

void NonogramActivity::activateIndex(const int index) {
  if (names.empty() || index < 0 || index >= listCount()) return;
  app.clearTapFlash();
  nav.selected = index;
  auto play = makeUniqueNoThrow<NonogramPlayActivity>(renderer, mappedInput, index);
  if (!play) {
    LOG_ERR("NONO", "OOM: play activity");
    return;
  }
  // Reload the ticks on return; this runs on the main loop, not the render task.
  startActivityForResult(std::move(play), [this](const ActivityResult&) { loadProgress(progress); });
}

void NonogramActivity::buildScreen(UiScreen& screen) {
  const auto& metrics = UITheme::getInstance().getMetrics();
  const Rect safe = UITheme::getInstance().getScreenSafeArea(renderer, true, false);
  screen.setContentMargin(fui::Insets{static_cast<int16_t>(safe.y + metrics.topPadding + metrics.headerHeight),
                                      static_cast<int16_t>(renderer.getScreenWidth() - (safe.x + safe.width)),
                                      static_cast<int16_t>(renderer.getScreenHeight() - (safe.y + safe.height)),
                                      static_cast<int16_t>(safe.x)});
  screen.spacer(static_cast<int16_t>(metrics.verticalSpacing));
  if (names.empty()) {
    rows[0].label = tr(STR_GAME_NONOGRAM_NONE);
    rows[0].enabled = false;
  }
  for (int i = 0; i < static_cast<int>(names.size()); ++i) {
    // The name gives the picture away, so it shows only once solved.
    const bool done = (progress.solved[i / 32] >> (i % 32)) & 1U;
    snprintf(numbers[i], sizeof(numbers[i]), "#%d", i + 1);
    rows[i].label = done ? names[i].c_str() : numbers[i];
    rows[i].value = done ? tr(STR_GAME_SOLVED) : nullptr;
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
