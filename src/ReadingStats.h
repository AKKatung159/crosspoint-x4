#pragma once

#include <cstdint>
#include <string>

/// PERSONAL: reading statistics (this fork, phase 5).
///
/// The reader reports a session (open a book, show pages, leave it or sleep).
/// Time counts only while pages keep turning: a gap between two pages adds at
/// most kMaxGapSec, so a book left open on the desk does not count. Everything
/// stays in RAM during the session and is written once, when the session ends,
/// to /.crosspoint/reading-stats.json (totals, the last 60 days, the last 40
/// books). Days need the clock; without it a session still counts in the totals.
/// DashboardSync uploads that file to the dashboard server and marks it synced.
namespace ReadingStats {

constexpr uint32_t kMaxGapSec = 180;
constexpr int kWeekDays = 7;

void begin(const std::string& bookPath, const std::string& title);
/// A page was shown (one call per page render).
void onPage();
/// The book's progress when the session ends (from the reader's home summary).
void setPercent(int percent);
/// Ends the session and saves; harmless without a session.
void end();

struct Day {
  char key[11] = {};  // "YYYY-MM-DD", empty if unknown
  uint32_t sec = 0;
  uint32_t pages = 0;
};

struct Summary {
  uint32_t totalSec = 0;
  uint32_t totalPages = 0;
  uint32_t sessions = 0;
  uint32_t booksFinished = 0;
  uint32_t streakDays = 0;  // consecutive days with reading, ending today or yesterday
  Day today;
  Day week[kWeekDays];  // oldest first, the last entry is today
  bool clockKnown = false;
  std::string lastTitle;  // most recently read book
  uint32_t lastBookSec = 0;
  int lastBookPercent = -1;
};

/// Reads the saved stats (plus nothing in flight). False if there are none yet.
bool summary(Summary& out);

/// The whole stats file for upload; false if missing or already synced.
bool pendingUpload(std::string& json);
void markSynced();

}  // namespace ReadingStats
