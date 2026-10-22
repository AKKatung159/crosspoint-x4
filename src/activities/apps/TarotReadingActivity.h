#pragma once

#include "activities/Activity.h"
#include "apps/TsvFile.h"

// PERSONAL: Tarot, step 2: draws `count` distinct cards, each upright or
// reversed. Confirm switches between the card image and its meaning;
// Left/Right (or Up/Down) move through a three-card spread.
class TarotReadingActivity final : public Activity {
 public:
  bool wantsTouchButtons() const override { return true; }  // PERSONAL-HOOK(touch-buttons)
  TarotReadingActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, int count)
      : Activity("TarotReading", renderer, mappedInput), count(count) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  static constexpr int MAX_CARDS = 3;
  // deck.tsv columns.
  enum Column { FILE, NAME_EN, NAME_TH, UP_EN, UP_TH, REV_EN, REV_TH, COLUMNS };

  const int count;
  TsvFile deck;
  bool deckOk = false;
  uint32_t drawn[MAX_CARDS] = {};
  bool reversed[MAX_CARDS] = {};
  int current = 0;
  bool showMeaning = false;
  char row[TsvFile::MAX_LINE + 1] = {};
  char* cols[COLUMNS] = {};

  void loadCard();
  void renderImage(int top, int bottom) const;
  void renderMeaning(int top, int bottom) const;
};
