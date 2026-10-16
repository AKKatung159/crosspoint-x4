#pragma once

#include "ReadingStats.h"
#include "activities/Activity.h"

// PERSONAL: the reading statistics screen (home tile "สถิติ"), drawn in the
// personal home themes' style: big numbers, solid grey cards, a 7-day chart.
class ReadingStatsActivity final : public Activity {
 public:
  explicit ReadingStatsActivity(GfxRenderer& renderer, MappedInputManager& mappedInput)
      : Activity("ReadingStats", renderer, mappedInput) {}

  void onEnter() override;
  void loop() override;
  void render(RenderLock&&) override;

 private:
  ReadingStats::Summary stats;
  bool haveStats = false;
};
