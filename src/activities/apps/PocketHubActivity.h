#pragma once

#include <I18n.h>

#include <vector>

#include "activities/UiListActivity.h"
#include "apps/sdk/AppManifest.h"

// PERSONAL: the Pocket Hub (home tile "Apps"): built-in mini apps plus every
// Pocket SDK app found on the card. Each app is an Activity pushed on top of
// the hub, so Back (finish()) returns here and sleep runs every app's onExit().
class PocketHubActivity final : public UiListActivity {
 public:
  PocketHubActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("PocketHub", renderer, mappedInput) {}
  void onEnter() override;

 private:
  static constexpr int MAX_APPS = 32;
  static constexpr int MAX_SDK_APPS = 16;
  freeink::ui::ListItem rows[MAX_APPS]{};
  std::vector<AppManifest> sdkApps;  // /apps/<folder>/manifest.json, scanned on entry

  int listCount() const override;
  const char* headerTitle() const override { return tr(STR_POCKET_HUB); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  // goHome() while the hub is current, so Home reselects the Apps tile.
  void onBackButton() override { activityManager.goHome(); }
};
