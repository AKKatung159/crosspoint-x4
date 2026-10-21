#pragma once

#include "NonogramActivity.h"
#include "activities/Activity.h"
#include "apps/Nonogram.h"

// PERSONAL: Nonogram, step 2: arrows move, Confirm fills or clears a cell,
// holding Confirm marks it with a cross. Marks are saved on exit.
class NonogramPlayActivity final : public Activity {
 public:
  NonogramPlayActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int index)
      : Activity("NonogramPlay", renderer, mappedInput), index(index) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  const int index;
  Nonogram puzzle;
  NonogramActivity::Progress progress;
  char name[64] = {};
  bool ok = false;
  bool solved = false;
  bool dirty = false;
  int cx = 0, cy = 0;
  int fastRefreshes = 0;
};
