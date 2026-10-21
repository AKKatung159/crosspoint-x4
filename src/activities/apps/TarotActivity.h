#pragma once

#include <I18n.h>

#include "activities/UiListActivity.h"

// PERSONAL: Tarot, step 1: pick a spread (one card, or past/present/future).
// The pack lives on SD: /apps/tarot/deck.tsv and /apps/tarot/cards/<file>.bmp.
class TarotActivity final : public UiListActivity {
 public:
  TarotActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("Tarot", renderer, mappedInput) {}

 private:
  freeink::ui::ListItem rows[2]{};

  int listCount() const override { return 2; }
  const char* headerTitle() const override { return tr(STR_TAROT); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
};
