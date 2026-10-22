#pragma once

#include "activities/Activity.h"
#include "apps/Game2048.h"

// PERSONAL: 2048 on the four arrow buttons (front Left/Right, side Up/Down).
// The board is saved on exit; Confirm starts a new game once no move is left.
class Game2048Activity final : public Activity {
 public:
  bool wantsTouchButtons() const override { return true; }  // PERSONAL-HOOK(touch-buttons)
  Game2048Activity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("Game2048", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  Game2048 game;
  bool dirty = false;
  int fastRefreshes = 0;

  void newGame();
};
