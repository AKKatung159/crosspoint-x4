#pragma once

#include <I18n.h>

#include <string>
#include <vector>

#include "activities/UiListActivity.h"

// PERSONAL: Flashcards, step 1: pick a deck (a .tsv file in /apps/flashcards).
class FlashcardsActivity final : public UiListActivity {
 public:
  FlashcardsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : UiListActivity("Flashcards", renderer, mappedInput) {}
  void onEnter() override;

 private:
  static constexpr int MAX_DECKS = 32;
  std::vector<std::string> decks;
  freeink::ui::ListItem rows[MAX_DECKS]{};

  int listCount() const override { return decks.empty() ? 1 : static_cast<int>(decks.size()); }
  const char* headerTitle() const override { return tr(STR_FLASHCARDS); }
  void buildScreen(UiScreen& screen) override;
  void activateIndex(int index) override;
};
