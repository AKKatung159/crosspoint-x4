#include "ReadingStatsActivity.h"

#include <GfxRenderer.h>
#include <I18n.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "MappedInputManager.h"
#include "activities/ActivityManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

// "1 ชม. 25 นาที" / "25 นาที"
std::string duration(const uint32_t sec) {
  const uint32_t min = sec / 60;
  char buf[48];
  if (min >= 60) {
    snprintf(buf, sizeof(buf), "%u %s %u %s", static_cast<unsigned>(min / 60), tr(STR_STATS_HOURS),
             static_cast<unsigned>(min % 60), tr(STR_STATS_MIN));
  } else {
    snprintf(buf, sizeof(buf), "%u %s", static_cast<unsigned>(min), tr(STR_STATS_MIN));
  }
  return buf;
}

// Short weekday from "YYYY-MM-DD" (Sakamoto), Thai or English by UI language.
const char* weekday(const char* key) {
  static const char* const th[] = {"อา", "จ", "อ", "พ", "พฤ", "ศ", "ส"};
  static const char* const en[] = {"Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"};
  int y = 0, m = 0, d = 0;
  if (sscanf(key, "%d-%d-%d", &y, &m, &d) != 3) return "";
  static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
  if (m < 3) y -= 1;
  const int w = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
  return I18N.getLanguage() == Language::TH ? th[w] : en[w];
}

constexpr int kCardH = 108;

// A grey card with a big value and a small caption under it.
void card(const GfxRenderer& r, const int x, const int y, const int w, const char* value, const char* caption) {
  r.fillRoundedRect(x, y, w, kCardH, 14, Color::LightGray);
  const std::string v = r.truncatedText(NOTOSANS_18_FONT_ID, value, w - 12, EpdFontFamily::BOLD);
  r.drawText(NOTOSANS_18_FONT_ID, x + (w - r.getTextWidth(NOTOSANS_18_FONT_ID, v.c_str(), EpdFontFamily::BOLD)) / 2,
             y + 10, v.c_str(), true, EpdFontFamily::BOLD);
  const std::string c = r.truncatedText(UI_10_FONT_ID, caption, w - 12);
  r.drawText(UI_10_FONT_ID, x + (w - r.getTextWidth(UI_10_FONT_ID, c.c_str())) / 2, y + 72, c.c_str());
}

}  // namespace

void ReadingStatsActivity::onEnter() {
  Activity::onEnter();
  haveStats = ReadingStats::summary(stats);
  requestUpdate();
}

void ReadingStatsActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back) ||
      mappedInput.wasReleased(MappedInputManager::Button::Confirm)) {
    finish();  // back to the Pocket Hub
  }
}

void ReadingStatsActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageW = renderer.getScreenWidth();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageW, metrics.headerHeight}, tr(STR_READING_STATS), nullptr);

  constexpr int side = 24;
  int y = metrics.topPadding + metrics.headerHeight + 16;
  const int innerW = pageW - 2 * side;

  if (!haveStats) {
    renderer.drawCenteredText(UI_12_FONT_ID, y + 200, tr(STR_STATS_NONE));
  } else {
    // Today: the one big number.
    renderer.drawText(UI_12_FONT_ID, side, y, tr(STR_STATS_TODAY));
    y += 30;
    const std::string today = stats.clockKnown ? duration(stats.today.sec) : "–";
    renderer.drawText(NOTOSANS_18_FONT_ID, side, y, today.c_str(), true, EpdFontFamily::BOLD);
    if (stats.clockKnown) {
      char pages[32];
      snprintf(pages, sizeof(pages), "%u %s", static_cast<unsigned>(stats.today.pages), tr(STR_STATS_PAGES));
      const int tw = renderer.getTextWidth(UI_12_FONT_ID, pages);
      renderer.drawText(UI_12_FONT_ID, pageW - side - tw, y + 8, pages);
    }
    y += 52;

    // Three cards: streak, finished, all time.
    const int gap = 12;
    const int cw = (innerW - 2 * gap) / 3;
    char streak[24], finished[24];
    snprintf(streak, sizeof(streak), "%u", static_cast<unsigned>(stats.streakDays));
    snprintf(finished, sizeof(finished), "%u", static_cast<unsigned>(stats.booksFinished));
    char total[24];
    snprintf(total, sizeof(total), "%u", static_cast<unsigned>(stats.totalSec / 3600));
    card(renderer, side, y, cw, streak, tr(STR_STATS_STREAK));
    card(renderer, side + cw + gap, y, cw, finished, tr(STR_STATS_FINISHED));
    card(renderer, side + 2 * (cw + gap), y, cw, total, tr(STR_STATS_TOTAL));
    y += kCardH + 24;

    // Last 7 days: bars, today solid black.
    renderer.drawText(UI_12_FONT_ID, side, y, tr(STR_STATS_WEEK));
    y += 34;
    constexpr int chartH = 150;
    if (stats.clockKnown) {
      uint32_t maxSec = 60;
      for (const auto& d : stats.week) maxSec = std::max(maxSec, d.sec);
      const int slot = innerW / ReadingStats::kWeekDays;
      const int barW = slot - 18;
      for (int i = 0; i < ReadingStats::kWeekDays; i++) {
        const auto& d = stats.week[i];
        const int x = side + i * slot + (slot - barW) / 2;
        const int h = d.sec == 0 ? 0 : std::max(6, static_cast<int>(static_cast<uint64_t>(d.sec) * chartH / maxSec));
        const bool isToday = i == ReadingStats::kWeekDays - 1;
        renderer.fillRectDither(x, y, barW, chartH, Color::LightGray);
        if (h > 0) {
          renderer.fillRoundedRect(x, y + chartH - h, barW, h, 6, isToday ? Color::Black : Color::DarkGray);
        }
        char mins[12];
        snprintf(mins, sizeof(mins), "%u", static_cast<unsigned>(d.sec / 60));
        if (d.sec > 0) {
          const int mw = renderer.getTextWidth(SMALL_FONT_ID, mins);
          renderer.drawText(SMALL_FONT_ID, x + (barW - mw) / 2, y + chartH - h - 18, mins);
        }
        const char* wd = weekday(d.key);
        const int ww = renderer.getTextWidth(UI_10_FONT_ID, wd, isToday ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
        renderer.drawText(UI_10_FONT_ID, x + (barW - ww) / 2, y + chartH + 6, wd, true,
                          isToday ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR);
      }
    } else {
      renderer.fillRoundedRect(side, y, innerW, chartH, 14, Color::LightGray);
      const std::string msg = renderer.truncatedText(UI_10_FONT_ID, tr(STR_STATS_NO_CLOCK), innerW - 20);
      renderer.drawCenteredText(UI_10_FONT_ID, y + chartH / 2 - 10, msg.c_str());
    }
    y += chartH + 44;

    // Last book: title, time in it, progress.
    if (!stats.lastTitle.empty()) {
      renderer.drawText(UI_12_FONT_ID, side, y, tr(STR_STATS_LAST_BOOK));
      y += 32;
      const std::string title =
          renderer.truncatedText(NOTOSANS_16_FONT_ID, stats.lastTitle.c_str(), innerW, EpdFontFamily::BOLD);
      renderer.drawText(NOTOSANS_16_FONT_ID, side, y, title.c_str(), true, EpdFontFamily::BOLD);
      y += 40;
      std::string line = duration(stats.lastBookSec);
      if (stats.lastBookPercent >= 0) line += " · " + std::to_string(stats.lastBookPercent) + "%";
      renderer.drawText(UI_10_FONT_ID, side, y, line.c_str());
      y += 30;
      renderer.fillRectDither(side, y, innerW, 10, Color::LightGray);
      if (stats.lastBookPercent > 0) {
        renderer.fillRect(side, y, innerW * std::min(stats.lastBookPercent, 100) / 100, 10, true);
      }
    }
  }

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}
