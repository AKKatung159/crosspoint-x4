#pragma once

#include <I18n.h>

#include <string>
#include <vector>

#include "activities/UiListActivity.h"
#include "apps/TsvFile.h"

// PERSONAL: Nonogram, step 1: pick a puzzle from /apps/nonogram/puzzles.tsv.
// Solved puzzles carry a tick.
class NonogramActivity final : public UiListActivity {
 public:
  NonogramActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("Nonogram", renderer, mappedInput) {}
  void onEnter() override;

  // Shared with the play screen: one save file for both.
  static constexpr int MAX_PUZZLES = 64;
  struct Progress {
    uint32_t solved[MAX_PUZZLES / 32] = {};
    uint16_t current = 0xFFFF;  // puzzle whose marks are saved
    uint8_t marks[15 * 15] = {};
  };
  static bool loadProgress(Progress& p);
  static bool saveProgress(const Progress& p);
  static bool openPuzzles(TsvFile& file);

 private:
  std::vector<std::string> names;
  Progress progress;
  freeink::ui::ListItem rows[MAX_PUZZLES]{};
  char numbers[MAX_PUZZLES][4] = {};  // "#1".."#64"

  int listCount() const override { return names.empty() ? 1 : static_cast<int>(names.size()); }
  const char* headerTitle() const override { return tr(STR_GAME_NONOGRAM); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
};
