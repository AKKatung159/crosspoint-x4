#include "DashboardSync.h"

#include <HTTPClient.h>
#include <HalClock.h>
#include <HalStorage.h>
#include <Logging.h>
#include <WiFi.h>

#include <ctime>
#include <string>

#include "CrossPointSettings.h"
#include "ReadingStats.h"
#include "WifiCredentialStore.h"
#include "network/HttpDownloader.h"

namespace DashboardSync {
namespace {

constexpr char kDefaultUrl[] = "http://x4.lan";
constexpr unsigned long kJoinTimeoutMs = 8000;
constexpr char kTmpPath[] = "/.crosspoint/dashboard.tmp";

// UTC time of the last good dashboard; RTC memory lasts through deep sleep.
RTC_DATA_ATTR time_t gLastDashboardUtc = 0;

std::string baseUrl() {
  std::string url = SETTINGS.dashboardUrl;
  while (!url.empty() && url.front() == ' ') url.erase(url.begin());
  if (url.empty()) url = kDefaultUrl;
  if (url.find("://") == std::string::npos) url = "http://" + url;
  while (!url.empty() && url.back() == '/') url.pop_back();
  return url;
}

bool join() {
  WIFI_STORE.loadFromFile();
  const std::string ssid = WIFI_STORE.getLastConnectedSsid();
  if (ssid.empty()) {
    LOG_INF("DASH", "No saved network yet");
    return false;
  }
  const auto cred = WIFI_STORE.findCredential(ssid);
  WiFi.mode(WIFI_STA);
  if (cred && !cred->password.empty()) {
    WiFi.begin(ssid.c_str(), cred->password.c_str());
  } else {
    WiFi.begin(ssid.c_str());
  }
  const unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < kJoinTimeoutMs) delay(100);
  if (WiFi.status() != WL_CONNECTED) {
    LOG_INF("DASH", "Could not join %s", ssid.c_str());
    return false;
  }
  LOG_INF("DASH", "Joined %s in %lu ms", ssid.c_str(), millis() - start);
  return true;
}

bool uploadStats(const std::string& base) {
  std::string json;
  if (!ReadingStats::pendingUpload(json)) return false;
  HTTPClient http;
  http.setTimeout(4000);
  if (!http.begin((base + "/x4/stats").c_str())) return false;
  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(reinterpret_cast<uint8_t*>(json.data()), json.size());
  http.end();
  if (code != 200) {
    LOG_INF("DASH", "Stats upload: HTTP %d", code);
    return false;
  }
  ReadingStats::markSynced();
  return true;
}

bool downloadDashboard(const std::string& base) {
  Storage.remove(kTmpPath);
  const auto err = HttpDownloader::downloadToFile(base + "/x4/dashboard.bmp", kTmpPath);
  if (err != HttpDownloader::OK) {
    LOG_INF("DASH", "Dashboard download failed (%d)", static_cast<int>(err));
    Storage.remove(kTmpPath);
    return false;
  }
  Storage.remove(kDashboardPath);
  if (!Storage.rename(kTmpPath, kDashboardPath)) return false;
  gLastDashboardUtc = time(nullptr);
  return true;
}

}  // namespace

Result run() {
  Result r;
  const time_t now = time(nullptr);
  if (gLastDashboardUtc != 0 && now > gLastDashboardUtc && now - gLastDashboardUtc < kMinIntervalSec &&
      Storage.exists(kDashboardPath)) {
    r.dashboard = true;  // fresh enough: no radio this time
    return r;
  }
  r.wifi = join();
  if (!r.wifi) return r;
  if (halClock.isSoftware() || halClock.secondsSinceSync() < 0) r.clock = halClock.syncFromNTP();
  const std::string base = baseUrl();
  r.statsUploaded = uploadStats(base);
  r.dashboard = downloadDashboard(base);
  LOG_INF("DASH", "Sync: clock=%d stats=%d dashboard=%d", r.clock, r.statsUploaded, r.dashboard);
  return r;
}

}  // namespace DashboardSync
