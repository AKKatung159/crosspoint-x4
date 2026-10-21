#pragma once

#include <I18n.h>

#include <vector>

#include "activities/Activity.h"
#include "apps/sdk/AppManifest.h"

// PERSONAL: the Pocket Hub (home tile "Apps"): built-in mini apps plus every
// Pocket SDK app found on the card, as a grid of icons three to a row. An SDK
// app hides the built-in it replaces. Each app is an Activity pushed on top of
// the hub, so Back (finish()) returns here and sleep runs every app's onExit().
class PocketHubActivity final : public Activity {
 public:
  PocketHubActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("PocketHub", renderer, mappedInput) {}
  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int MAX_APPS = 32;
  static constexpr int MAX_SDK_APPS = 16;
  static constexpr int COLUMNS = 3;
  std::vector<AppManifest> sdkApps;  // /apps/<folder>/manifest.json, scanned on entry
  uint8_t builtIn[MAX_APPS] = {};    // indexes into the built-in table that are shown
  int builtIns = 0;
  int selected = 0;
  int firstRow = 0;  // the grid scrolls by rows when it is taller than the screen
  int fastRefreshes = 0;

  int count() const;
  const char* name(int index) const;
  void open(int index);
  void move(int delta);
  void drawCell(int index, int x, int y, int w, int h) const;
};
