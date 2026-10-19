#pragma once

#include <string>

#include "activities/Activity.h"
#include "apps/TsvFile.h"

// Decks are .tsv files here, one "front<TAB>back" card per line.
constexpr char DECK_DIR[] = "/apps/flashcards";

// PERSONAL: Flashcards, step 2: one card at a time. Confirm flips, Up/Down and
// Left/Right move. The position in each recent deck is saved on exit.
class FlashcardStudyActivity final : public Activity {
 public:
  FlashcardStudyActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, std::string deckName)
      : Activity("FlashcardStudy", renderer, mappedInput), deckName(std::move(deckName)) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  // Last positions of up to RECENT decks, keyed by TsvFile::nameHash of the deck name.
  static constexpr int RECENT = 8;
  struct Progress {
    uint32_t hash[RECENT] = {};
    uint32_t pos[RECENT] = {};
    uint32_t next = 0;  // slot reused by the next new deck
  };

  const std::string deckName;
  TsvFile deck;
  bool deckOk = false;
  uint32_t index = 0;
  bool flipped = false;
  bool moved = false;
  int fastRefreshes = 0;
  char line[TsvFile::MAX_LINE + 1] = {};
  const char* front = "";
  const char* back = "";

  void loadCard();
};
