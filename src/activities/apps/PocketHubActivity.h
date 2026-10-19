#pragma once

#include <I18n.h>

#include "activities/UiListActivity.h"

// PERSONAL: the Pocket Hub, this fork's launcher for built-in mini apps (home
// tile "Apps"). Each app is an ordinary Activity pushed on top of the hub, so
// Back (finish()) returns here and sleep runs every app's onExit().
class PocketHubActivity final : public UiListActivity {
 public:
  PocketHubActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("PocketHub", renderer, mappedInput) {}

 private:
  static constexpr int MAX_APPS = 8;
  freeink::ui::ListItem rows[MAX_APPS]{};

  int listCount() const override;
  const char* headerTitle() const override { return tr(STR_POCKET_HUB); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
  // goHome() while the hub is current, so Home reselects the Apps tile.
  void onBackButton() override { activityManager.goHome(); }
};
