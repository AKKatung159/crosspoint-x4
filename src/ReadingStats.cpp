#include "ReadingStats.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <Logging.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <vector>

namespace ReadingStats {
namespace {

constexpr char kPath[] = "/.crosspoint/reading-stats.json";
constexpr size_t kKeepDays = 60;
constexpr size_t kKeepBooks = 40;
constexpr uint32_t kFinishedPercent = 99;

struct Session {
  bool active = false;
  std::string path;
  std::string title;
  unsigned long lastEventMs = 0;
  uint32_t sec = 0;
  uint32_t pages = 0;
  int percent = -1;
  uint32_t msRemainder = 0;
} session;

void addGap(const unsigned long nowMs) {
  const unsigned long gap = std::min<unsigned long>(nowMs - session.lastEventMs, kMaxGapSec * 1000UL);
  session.msRemainder += gap;
  session.sec += session.msRemainder / 1000;
  session.msRemainder %= 1000;
  session.lastEventMs = nowMs;
}

bool dayKey(const time_t t, char (&out)[11]) {
  struct tm local;
  localtime_r(&t, &local);
  if (local.tm_year + 1900 < 2025) return false;
  strftime(out, sizeof(out), "%Y-%m-%d", &local);
  return true;
}

bool todayKey(char (&out)[11]) {
  struct tm local;
  if (!halClock.localTime(local)) return false;
  strftime(out, sizeof(out), "%Y-%m-%d", &local);
  return true;
}

bool load(JsonDocument& doc) {
  if (!Storage.exists(kPath)) return false;
  const String text = Storage.readFile(kPath);
  if (text.isEmpty()) return false;
  if (deserializeJson(doc, text)) {
    LOG_ERR("STATS", "Unreadable %s, starting over", kPath);
    doc.clear();
    return false;
  }
  return true;
}

// Drops the oldest entries of an object keyed so that larger = newer.
void trimOldest(JsonObject obj, const size_t keep, const char* field = nullptr) {
  while (obj.size() > keep) {
    const char* oldestKey = nullptr;
    std::string oldestValue;
    for (JsonPair kv : obj) {
      const std::string v = field ? std::string(kv.value()[field] | "") : std::string(kv.key().c_str());
      if (!oldestKey || v < oldestValue) {
        oldestKey = kv.key().c_str();
        oldestValue = v;
      }
    }
    if (!oldestKey) break;
    obj.remove(std::string(oldestKey));
  }
}

}  // namespace

void begin(const std::string& bookPath, const std::string& title) {
  if (session.active) end();
  session = Session{};
  session.active = true;
  session.path = bookPath;
  session.title = title;
  session.lastEventMs = millis();
}

void onPage() {
  if (!session.active) return;
  addGap(millis());
  session.pages++;
}

void setPercent(const int percent) {
  if (session.active) session.percent = percent;
}

void end() {
  if (!session.active) return;
  addGap(millis());
  session.active = false;
  if (session.pages == 0 && session.sec < 5) return;  // opened and closed: not a session

  JsonDocument doc;
  load(doc);
  doc["v"] = 1;
  doc["sec"] = (doc["sec"] | 0u) + session.sec;
  doc["pages"] = (doc["pages"] | 0u) + session.pages;
  doc["sessions"] = (doc["sessions"] | 0u) + 1;

  char today[11] = {};
  const bool haveDay = todayKey(today);
  if (haveDay) {
    JsonObject days = doc["days"].is<JsonObject>() ? doc["days"].as<JsonObject>() : doc["days"].to<JsonObject>();
    JsonArray d = days[today].is<JsonArray>() ? days[today].as<JsonArray>() : days[today].to<JsonArray>();
    const uint32_t sec = (d[0] | 0u) + session.sec;
    const uint32_t pages = (d[1] | 0u) + session.pages;
    d.clear();
    d.add(sec);
    d.add(pages);
    trimOldest(days, kKeepDays);
  }

  JsonObject books = doc["books"].is<JsonObject>() ? doc["books"].as<JsonObject>() : doc["books"].to<JsonObject>();
  JsonObject b = books[session.path].is<JsonObject>() ? books[session.path].as<JsonObject>()
                                                      : books[session.path].to<JsonObject>();
  b["t"] = session.title;
  b["sec"] = (b["sec"] | 0u) + session.sec;
  b["pages"] = (b["pages"] | 0u) + session.pages;
  b["n"] = (b["n"] | 0u) + 1;
  if (session.percent >= 0) b["pct"] = session.percent;
  // "last" orders the books; a counter keeps that order even without a clock.
  const uint32_t seq = (doc["seq"] | 0u) + 1;
  doc["seq"] = seq;
  char last[24];
  snprintf(last, sizeof(last), "%010u", static_cast<unsigned>(seq));
  b["last"] = last;
  if (session.percent >= static_cast<int>(kFinishedPercent) && !b["done"].is<const char*>()) {
    b["done"] = haveDay ? today : "?";
    doc["finished"] = (doc["finished"] | 0u) + 1;
  }
  doc["lastBook"] = session.path;
  trimOldest(books, kKeepBooks, "last");
  doc["synced"] = false;

  String out;
  serializeJson(doc, out);
  if (!Storage.writeFile(kPath, out)) LOG_ERR("STATS", "Could not write %s", kPath);
  LOG_INF("STATS", "Session: %us, %u pages (%s)", static_cast<unsigned>(session.sec),
          static_cast<unsigned>(session.pages), haveDay ? today : "no clock");
}

bool summary(Summary& out) {
  out = Summary{};
  JsonDocument doc;
  if (!load(doc)) return false;
  out.totalSec = doc["sec"] | 0u;
  out.totalPages = doc["pages"] | 0u;
  out.sessions = doc["sessions"] | 0u;
  out.booksFinished = doc["finished"] | 0u;

  const char* lastPath = doc["lastBook"] | "";
  JsonObjectConst lb = doc["books"][lastPath];
  if (!lb.isNull()) {
    out.lastTitle = lb["t"] | "";
    out.lastBookSec = lb["sec"] | 0u;
    out.lastBookPercent = lb["pct"] | -1;
  }

  struct tm local;
  out.clockKnown = halClock.localTime(local);
  if (!out.clockKnown) return true;
  const time_t now = time(nullptr);
  JsonObjectConst days = doc["days"];
  for (int i = 0; i < kWeekDays; i++) {
    Day& d = out.week[i];
    if (!dayKey(now - static_cast<time_t>(kWeekDays - 1 - i) * 86400, d.key)) continue;
    JsonArrayConst v = days[d.key];
    d.sec = v[0] | 0u;
    d.pages = v[1] | 0u;
  }
  out.today = out.week[kWeekDays - 1];

  // Streak: count back from today (or yesterday, if nothing yet today).
  for (int back = out.today.sec > 0 ? 0 : 1; back < static_cast<int>(kKeepDays); back++) {
    char key[11];
    if (!dayKey(now - static_cast<time_t>(back) * 86400, key)) break;
    JsonArrayConst v = days[key];
    if ((v[0] | 0u) == 0) break;
    out.streakDays++;
  }
  return true;
}

bool pendingUpload(std::string& json) {
  JsonDocument doc;
  if (!load(doc) || (doc["synced"] | false)) return false;
  json.clear();
  serializeJson(doc, json);
  return true;
}

void markSynced() {
  JsonDocument doc;
  if (!load(doc)) return;
  doc["synced"] = true;
  String out;
  serializeJson(doc, out);
  Storage.writeFile(kPath, out);
}

}  // namespace ReadingStats
