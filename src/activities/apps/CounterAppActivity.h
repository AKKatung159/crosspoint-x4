#pragma once

#include "activities/Activity.h"

// PERSONAL, test builds only: the smallest Pocket Hub app. A number that
// Up/Confirm raise and Down lowers, saved through app_save on exit, so device
// and emulator runs can check the save path across Back, sleep and wake.
class CounterAppActivity final : public Activity {
 public:
  bool wantsTouchButtons() const override { return true; }  // PERSONAL-HOOK(touch-buttons)
  CounterAppActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("CounterApp", renderer, mappedInput) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  int32_t count = 0;
  bool dirty = false;
};
