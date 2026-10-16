#pragma once

/// PERSONAL: the X4's trip to its dashboard server (e.g. x4-hub, see server/).
///
/// Runs when the device goes to sleep with the "Dashboard" sleep screen:
/// join the last Wi-Fi network used (its saved password), set the clock from
/// NTP, upload the reading stats if they changed, and download the dashboard
/// image to kDashboardPath. Everything has a short time budget; on any failure
/// the last dashboard (or the book cover) is shown instead.
///
/// The server is Settings → Display → Dashboard URL (default http://x4.lan); it
/// must answer GET /x4/dashboard.bmp (480x800 BMP) and may take POST /x4/stats.
namespace DashboardSync {

constexpr char kDashboardPath[] = "/.crosspoint/dashboard.bmp";
/// Skip the trip when the last dashboard is younger than this (wake/sleep loops).
constexpr long kMinIntervalSec = 5 * 60;

struct Result {
  bool wifi = false;
  bool clock = false;
  bool statsUploaded = false;
  bool dashboard = false;
};

Result run();

}  // namespace DashboardSync
