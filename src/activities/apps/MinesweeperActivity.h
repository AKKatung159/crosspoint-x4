#pragma once

#include "activities/Activity.h"
#include "apps/Minesweeper.h"

// PERSONAL: Minesweeper. Arrows move the cursor, Confirm opens a cell, holding
// Confirm flags it. The game is saved on exit; Confirm starts a new one after
// a win or loss.
class MinesweeperActivity final : public Activity {
 public:
  bool wantsTouchButtons() const override { return true; }  // PERSONAL-HOOK(touch-buttons)
  MinesweeperActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Minesweeper", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  Minesweeper game;
  int cx = Minesweeper::W / 2;
  int cy = Minesweeper::H / 2;
  bool dirty = false;
  int fastRefreshes = 0;
};
