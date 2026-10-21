#include "WasmAppActivity.h"

#include <Arduino.h>
#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalMemory.h>
#include <HalStorage.h>
#include <I18n.h>
#include <Memory.h>
#include <m3_x4.h>
#include <wasm3.h>

#include <cstdio>
#include <cstring>

#include "DashboardSync.h"
#include "MappedInputManager.h"
#include "ReadingStats.h"
#include "activities/reader/ReaderUtils.h"
#include "apps/AppSave.h"
#include "apps/AppText.h"
#include "apps/sdk/AppPath.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

constexpr size_t MAX_MODULE_BYTES = 64 * 1024;
constexpr uint32_t WASM_STACK_BYTES = 4096;
// Work budget per call into the app: far above any real frame, low enough
// that a stuck loop ends in well under the watchdog's 5 s.
constexpr uint64_t GAS_PER_CALL = 20 * 1000 * 1000;
constexpr int FAST_REFRESHES_PER_HALF = 15;
constexpr uint16_t SAVE_VERSION = 1;  // the payload is the app's own format
constexpr int HINT_LEN = 48;          // 15 Thai characters (3 bytes each) and the NUL

constexpr int FONTS[] = {SMALL_FONT_ID, UI_10_FONT_ID, UI_12_FONT_ID, NOTOSANS_16_FONT_ID, NOTOSANS_18_FONT_ID};
int fontId(const int font) { return FONTS[font >= 0 && font < 5 ? font : 1]; }
EpdFontFamily::Style style(const int flags) { return flags & 1 ? EpdFontFamily::BOLD : EpdFontFamily::REGULAR; }
Color color(const int c) {
  static constexpr Color COLORS[] = {Color::White, Color::LightGray, Color::DarkGray, Color::Black};
  return COLORS[c >= 0 && c < 4 ? c : 3];
}

}  // namespace

struct WasmAppActivity::Runtime {
  const AppManifest* manifest = nullptr;
  GfxRenderer* renderer = nullptr;
  bool inRender = false;                           // drawing calls are ignored outside render()
  int refreshMode = 0;                             // set by x4_refresh during render()
  int dashX = 0, dashY = 0, dashW = 0, dashH = 0;  // x4_dashboard's box, drawn again in grey after the refresh
  int top = 0, bottom = 0;
  char title[64] = {};
  char hints[4][HINT_LEN] = {};
  char error[96] = {};

  std::unique_ptr<uint8_t[]> bytes;
  IM3Environment env = nullptr;
  IM3Runtime runtime = nullptr;
  IM3Function init = nullptr, event = nullptr, render = nullptr, exit = nullptr;

  ~Runtime() {
    if (runtime) m3_FreeRuntime(runtime);
    if (env) m3_FreeEnvironment(env);
  }
};

namespace {

using Rt = WasmAppActivity::Runtime;
Rt& host(IM3Runtime runtime) { return *static_cast<Rt*>(m3_GetUserData(runtime)); }

// A NUL-terminated string inside linear memory, or "" when it runs off the end.
const char* str(const void* mem, const char* s) {
  const size_t size = m3_GetMemorySizeAt(mem);
  const auto* base = static_cast<const char*>(mem);
  if (s < base || s >= base + size || !memchr(s, '\0', base + size - s)) return "";
  return s;
}

// Copies src into dst, cut at a UTF-8 character boundary when it does not fit.
void copyUtf8(char* dst, const size_t size, const char* src) {
  size_t n = strlen(src);
  if (n >= size) {
    n = size - 1;
    while (n > 0 && (static_cast<uint8_t>(src[n]) & 0xC0) == 0x80) --n;  // back off a continuation byte
  }
  memcpy(dst, src, n);
  dst[n] = '\0';
}

void textAt(const GfxRenderer& r, const int x, const int y, const char* s, const int font, const int flags) {
  const int id = fontId(font);
  const int tx = flags & 4 ? x - r.getTextWidth(id, s, style(flags)) / 2 : x;
  r.drawText(id, tx, y, s, !(flags & 2), style(flags));
}

// --- display ---------------------------------------------------------------
m3ApiRawFunction(xFill) {
  m3ApiGetArg(int32_t, x) m3ApiGetArg(int32_t, y) m3ApiGetArg(int32_t, w) m3ApiGetArg(int32_t, h)
      m3ApiGetArg(int32_t, c) Rt& h_ = host(runtime);
  if (h_.inRender && w > 0 && h > 0) {
    if (c == 0 || c == 3) {
      h_.renderer->fillRect(x, y, w, h, c == 3);
    } else {
      h_.renderer->fillRectDither(x, y, w, h, color(c));
    }
  }
  m3ApiSuccess();
}
m3ApiRawFunction(xRfill) {
  m3ApiGetArg(int32_t, x) m3ApiGetArg(int32_t, y) m3ApiGetArg(int32_t, w) m3ApiGetArg(int32_t, h)
      m3ApiGetArg(int32_t, radius) m3ApiGetArg(int32_t, c) Rt& h_ = host(runtime);
  if (h_.inRender && w > 0 && h > 0) h_.renderer->fillRoundedRect(x, y, w, h, radius, color(c));
  m3ApiSuccess();
}
m3ApiRawFunction(xFrame) {
  m3ApiGetArg(int32_t, x) m3ApiGetArg(int32_t, y) m3ApiGetArg(int32_t, w) m3ApiGetArg(int32_t, h)
      m3ApiGetArg(int32_t, thick) m3ApiGetArg(int32_t, c) Rt& h_ = host(runtime);
  if (h_.inRender && w > 0 && h > 0) h_.renderer->drawRect(x, y, w, h, thick > 0 ? thick : 1, c >= 2);
  m3ApiSuccess();
}
m3ApiRawFunction(xLine) {
  m3ApiGetArg(int32_t, x1) m3ApiGetArg(int32_t, y1) m3ApiGetArg(int32_t, x2) m3ApiGetArg(int32_t, y2)
      m3ApiGetArg(int32_t, width) m3ApiGetArg(int32_t, c) Rt& h_ = host(runtime);
  if (h_.inRender) h_.renderer->drawLine(x1, y1, x2, y2, width > 0 ? width : 1, c >= 2);
  m3ApiSuccess();
}
m3ApiRawFunction(xText) {
  m3ApiGetArg(int32_t, x) m3ApiGetArg(int32_t, y) m3ApiGetArgMem(const char*, s) m3ApiGetArg(int32_t, font)
      m3ApiGetArg(int32_t, flags) Rt& h_ = host(runtime);
  if (h_.inRender) textAt(*h_.renderer, x, y, str(_mem, s), font, flags);
  m3ApiSuccess();
}
m3ApiRawFunction(xTextW) {
  m3ApiReturnType(int32_t) m3ApiGetArgMem(const char*, s) m3ApiGetArg(int32_t, font) m3ApiGetArg(int32_t, flags)
      m3ApiReturn(host(runtime).renderer->getTextWidth(fontId(font), str(_mem, s), style(flags)));
}
m3ApiRawFunction(xLineH) {
  m3ApiReturnType(int32_t) m3ApiGetArg(int32_t, font) m3ApiReturn(host(runtime).renderer->getLineHeight(fontId(font)));
}
// Wrapped text (Thai-aware) in a box of width w from (x, y); returns the lines drawn.
m3ApiRawFunction(xTextBox) {
  m3ApiReturnType(int32_t) m3ApiGetArg(int32_t, x) m3ApiGetArg(int32_t, y) m3ApiGetArg(int32_t, w)
      m3ApiGetArg(int32_t, maxLines) m3ApiGetArgMem(const char*, s) m3ApiGetArg(int32_t, font)
          m3ApiGetArg(int32_t, flags) Rt& h_ = host(runtime);
  if (!h_.inRender || w <= 0 || maxLines <= 0) m3ApiReturn(0);
  const auto lines = app_text::wrap(*h_.renderer, fontId(font), str(_mem, s), w, maxLines, style(flags));
  const int lh = h_.renderer->getLineHeight(fontId(font));
  int ly = y;
  for (const auto& l : lines) {
    textAt(*h_.renderer, flags & 4 ? x + w / 2 : x, ly, l.c_str(), font, flags);
    ly += lh;
  }
  m3ApiReturn(static_cast<int32_t>(lines.size()));
}
// A BMP from the app's folder, scaled down into maxW x maxH; 1 when drawn.
m3ApiRawFunction(xImage) {
  m3ApiReturnType(int32_t) m3ApiGetArgMem(const char*, rel) m3ApiGetArg(int32_t, x) m3ApiGetArg(int32_t, y)
      m3ApiGetArg(int32_t, maxW) m3ApiGetArg(int32_t, maxH) Rt& h_ = host(runtime);
  char path[112];
  if (!h_.inRender || !app_path::resolve(h_.manifest->dir, str(_mem, rel), path, sizeof(path)) ||
      !Storage.exists(path)) {
    m3ApiReturn(0);
  }
  HalFile file;
  if (!Storage.openFileForRead("SDK", path, file)) m3ApiReturn(0);
  Bitmap bitmap(file);
  if (bitmap.parseHeaders() != BmpReaderError::Ok) m3ApiReturn(0);
  m3ApiReturn(h_.renderer->drawBitmap(bitmap, x, y, maxW, maxH) ? 1 : 0);
}
// x4_image with flags; X4_ROTATE180 turns a 1-bit BMP upside down in the same box.
m3ApiRawFunction(xImage2) {
  m3ApiReturnType(int32_t) m3ApiGetArgMem(const char*, rel) m3ApiGetArg(int32_t, x) m3ApiGetArg(int32_t, y)
      m3ApiGetArg(int32_t, maxW) m3ApiGetArg(int32_t, maxH) m3ApiGetArg(int32_t, flags) Rt& h_ = host(runtime);
  char path[112];
  if (!h_.inRender || !app_path::resolve(h_.manifest->dir, str(_mem, rel), path, sizeof(path)) ||
      !Storage.exists(path)) {
    m3ApiReturn(0);
  }
  HalFile file;
  if (!Storage.openFileForRead("SDK", path, file)) m3ApiReturn(0);
  Bitmap bitmap(file);
  if (bitmap.parseHeaders() != BmpReaderError::Ok) m3ApiReturn(0);
  const bool rotate = (flags & 1) && bitmap.is1Bit();
  const bool drawn = rotate ? h_.renderer->drawBitmap1Bit(bitmap, x, y, maxW, maxH, true)
                            : h_.renderer->drawBitmap(bitmap, x, y, maxW, maxH);
  m3ApiReturn(drawn ? 1 : 0);
}
m3ApiRawFunction(xRefresh) {
  m3ApiGetArg(int32_t, mode) host(runtime).refreshMode = mode;
  m3ApiSuccess();
}
m3ApiRawFunction(xTitle) {
  m3ApiGetArgMem(const char*, s) Rt& h_ = host(runtime);
  copyUtf8(h_.title, sizeof(h_.title), str(_mem, s));
  m3ApiSuccess();
}
// Labels for the four front buttons (Back, Confirm, Left, Right); 0 leaves one empty.
m3ApiRawFunction(xHints) {
  m3ApiGetArgMem(const char*, a) m3ApiGetArgMem(const char*, b) m3ApiGetArgMem(const char*, c)
      m3ApiGetArgMem(const char*, d) Rt& h_ = host(runtime);
  const char* labels[4] = {a, b, c, d};
  const auto* base = static_cast<const char*>(_mem);
  for (int i = 0; i < 4; ++i) copyUtf8(h_.hints[i], HINT_LEN, labels[i] == base ? "" : str(_mem, labels[i]));
  m3ApiSuccess();
}
m3ApiRawFunction(xWidth) { m3ApiReturnType(int32_t) m3ApiReturn(host(runtime).renderer->getScreenWidth()); }
m3ApiRawFunction(xTop) { m3ApiReturnType(int32_t) m3ApiReturn(host(runtime).top); }
m3ApiRawFunction(xBottom) { m3ApiReturnType(int32_t) m3ApiReturn(host(runtime).bottom); }

// --- storage ---------------------------------------------------------------
void saveId(const Rt& h, char* out, const size_t size) { snprintf(out, size, "sdk-%s", h.manifest->id); }
m3ApiRawFunction(xSave) {
  m3ApiReturnType(int32_t) m3ApiGetArgMem(const void*, data) m3ApiGetArg(int32_t, len) m3ApiCheckMem(data, len);
  Rt& h_ = host(runtime);
  if (len < 0 || len > h_.manifest->saveBytes) m3ApiReturn(0);
  char id[40];
  saveId(h_, id, sizeof(id));
  m3ApiReturn(app_save::save(id, SAVE_VERSION, data, len, h_.manifest->saveBytes) ? 1 : 0);
}
// Copies the last save into data (at most cap bytes); returns its length, or -1.
m3ApiRawFunction(xLoad) {
  m3ApiReturnType(int32_t) m3ApiGetArgMem(void*, data) m3ApiGetArg(int32_t, cap) m3ApiCheckMem(data, cap);
  Rt& h_ = host(runtime);
  if (h_.manifest->saveBytes == 0 || cap < h_.manifest->saveBytes) m3ApiReturn(-1);
  char id[40];
  saveId(h_, id, sizeof(id));
  size_t len = 0;
  m3ApiReturn(app_save::load(id, SAVE_VERSION, data, h_.manifest->saveBytes, &len) ? static_cast<int32_t>(len) : -1);
}
m3ApiRawFunction(xFsize) {
  m3ApiReturnType(int32_t) m3ApiGetArgMem(const char*, rel) Rt& h_ = host(runtime);
  char path[112];
  HalFile f;
  if (!app_path::resolve(h_.manifest->dir, str(_mem, rel), path, sizeof(path)) || !Storage.exists(path) ||
      !Storage.openFileForRead("SDK", path, f)) {
    m3ApiReturn(-1);
  }
  m3ApiReturn(static_cast<int32_t>(f.fileSize()));
}
// Reads up to len bytes at offset of a file in the app's folder; returns the count, or -1.
m3ApiRawFunction(xRead) {
  m3ApiReturnType(int32_t) m3ApiGetArgMem(const char*, rel) m3ApiGetArg(int32_t, offset) m3ApiGetArgMem(void*, out)
      m3ApiGetArg(int32_t, len) m3ApiCheckMem(out, len);
  Rt& h_ = host(runtime);
  char path[112];
  HalFile f;
  if (offset < 0 || len < 0 || !app_path::resolve(h_.manifest->dir, str(_mem, rel), path, sizeof(path)) ||
      !Storage.exists(path) || !Storage.openFileForRead("SDK", path, f) || !f.seek(offset)) {
    m3ApiReturn(-1);
  }
  m3ApiReturn(f.read(out, len));
}

// --- misc ------------------------------------------------------------------
m3ApiRawFunction(xRnd) { m3ApiReturnType(uint32_t) m3ApiReturn(esp_random()); }
m3ApiRawFunction(xMillis) { m3ApiReturnType(uint32_t) m3ApiReturn(static_cast<uint32_t>(millis())); }
m3ApiRawFunction(xLang) { m3ApiReturnType(int32_t) m3ApiReturn(I18N.getLanguage() == Language::TH ? 1 : 0); }
m3ApiRawFunction(xLog) {
  m3ApiGetArgMem(const char*, s) LOG_INF("APP", "%s: %s", host(runtime).manifest->id, str(_mem, s));
  m3ApiSuccess();
}

// --- v2 --------------------------------------------------------------------
// The n-th listable file of the app's folder (see x4_list in x4.h).
m3ApiRawFunction(xList) {
  m3ApiReturnType(int32_t) m3ApiGetArg(int32_t, n) m3ApiGetArgMem(char*, out) m3ApiGetArg(int32_t, cap)
      m3ApiCheckMem(out, cap);
  Rt& h_ = host(runtime);
  auto dir = Storage.open(h_.manifest->dir);
  if (n < 0 || cap <= 0 || !dir || !dir.isDirectory()) m3ApiReturn(-1);
  char name[64];
  int i = 0;
  for (auto f = dir.openNextFile(); f; f = dir.openNextFile()) {
    if (f.isDirectory() || !f.getName(name, sizeof(name)) || name[0] == '.' || !app_path::valid(name)) continue;
    if (i++ < n) continue;
    const int len = static_cast<int>(strlen(name));
    if (len >= cap) m3ApiReturn(-1);
    memcpy(out, name, len + 1);
    m3ApiReturn(len);
  }
  m3ApiReturn(-1);
}
// Row offsets of a TSV file in the app's folder (see x4_rows in x4.h). Scanning
// here keeps a long file off the app's gas budget.
m3ApiRawFunction(xRows) {
  m3ApiReturnType(int32_t) m3ApiGetArgMem(const char*, rel) m3ApiGetArgMem(uint32_t*, out) m3ApiGetArg(int32_t, max)
      m3ApiCheckMem(out, max > 0 ? static_cast<size_t>(max) * 4 : 0);
  Rt& h_ = host(runtime);
  char path[112];
  HalFile f;
  if (max < 0 || !app_path::resolve(h_.manifest->dir, str(_mem, rel), path, sizeof(path)) || !Storage.exists(path) ||
      !Storage.openFileForRead("SDK", path, f)) {
    m3ApiReturn(-1);
  }
  constexpr size_t CHUNK = 512;
  auto chunk = makeUniqueNoThrow<uint8_t[]>(CHUNK);  // heap: keeps the loop task's stack small
  if (!chunk) {
    LOG_ERR("SDK", "OOM: %u bytes", static_cast<unsigned>(CHUNK));
    m3ApiReturn(-1);
  }
  int32_t rows = 0;
  uint32_t offset = 0, lineStart = 0;
  char head[4];
  size_t lineLen = 0;
  bool sawTab = false;
  for (;;) {
    const int got = f.read(chunk.get(), CHUNK);
    if (got < 0) m3ApiReturn(-1);
    const bool eof = got == 0;  // a last line without '\n' still ends here
    for (int i = 0; i < (eof ? 1 : got); ++i) {
      const char c = eof ? '\n' : static_cast<char>(chunk[i]);
      if (c != '\n') {
        if (lineLen < sizeof(head)) head[lineLen] = c;
        sawTab |= c == '\t';
        ++lineLen;
        continue;
      }
      const size_t skip = lineLen >= 3 && memcmp(head, "\xEF\xBB\xBF", 3) == 0 ? 3 : 0;
      if (sawTab && lineLen > skip && head[skip] != '#') {
        if (rows == max) m3ApiReturn(rows);
        const uint32_t at = lineStart + static_cast<uint32_t>(skip);
        memcpy(out + rows++, &at, 4);
      }
      lineStart = offset + static_cast<uint32_t>(i) + 1;
      lineLen = 0;
      sawTab = false;
    }
    if (eof) break;
    offset += static_cast<uint32_t>(got);
  }
  m3ApiReturn(rows);
}
// x4_stats_t in x4.h: the same layout on wasm32 and on the ESP32 (all fields 4-byte aligned).
struct SdkStats {
  uint32_t totalSec, totalPages, sessions, finished, streak;
  int32_t clockKnown;
  struct {
    char key[12];
    uint32_t sec, pages;
  } week[ReadingStats::kWeekDays];
  uint32_t lastSec;
  int32_t lastPercent;
  char lastTitle[96];
};
static_assert(sizeof(SdkStats) == 268, "must match x4_stats_t");
m3ApiRawFunction(xStats) {
  m3ApiReturnType(int32_t) m3ApiGetArgMem(SdkStats*, out) m3ApiCheckMem(out, sizeof(SdkStats));
  auto s = makeUniqueNoThrow<ReadingStats::Summary>();  // heap: the Summary holds a std::string and 7 days
  if (!s || !ReadingStats::summary(*s)) m3ApiReturn(-1);
  SdkStats v = {};
  v.totalSec = s->totalSec;
  v.totalPages = s->totalPages;
  v.sessions = s->sessions;
  v.finished = s->booksFinished;
  v.streak = s->streakDays;
  v.clockKnown = s->clockKnown ? 1 : 0;
  for (int i = 0; i < ReadingStats::kWeekDays; ++i) {
    snprintf(v.week[i].key, sizeof(v.week[i].key), "%s", s->week[i].key);
    v.week[i].sec = s->week[i].sec;
    v.week[i].pages = s->week[i].pages;
  }
  v.lastSec = s->lastBookSec;
  v.lastPercent = s->lastBookPercent;
  snprintf(v.lastTitle, sizeof(v.lastTitle), "%s", s->lastTitle.c_str());
  memcpy(out, &v, sizeof(v));
  m3ApiReturn(static_cast<int32_t>(sizeof(v)));
}
m3ApiRawFunction(xDashboard) {
  m3ApiReturnType(int32_t) m3ApiGetArg(int32_t, x) m3ApiGetArg(int32_t, y) m3ApiGetArg(int32_t, maxW)
      m3ApiGetArg(int32_t, maxH) Rt& h_ = host(runtime);
  HalFile file;
  if (!h_.inRender || !Storage.exists(DashboardSync::kDashboardPath) ||
      !Storage.openFileForRead("SDK", DashboardSync::kDashboardPath, file)) {
    m3ApiReturn(0);
  }
  Bitmap bitmap(file);
  if (bitmap.parseHeaders() != BmpReaderError::Ok || !h_.renderer->drawBitmap(bitmap, x, y, maxW, maxH)) m3ApiReturn(0);
  h_.dashX = x;
  h_.dashY = y;
  h_.dashW = maxW;
  h_.dashH = maxH;
  m3ApiReturn(1);
}
// The B/W pass draws every non-white pixel of a 2-bit image black; this redraws
// the dashboard into the grey planes, like the home screen's grey covers.
void dashboardGreyPass(GfxRenderer& renderer, const Rt& h) {
  ReaderUtils::renderAntiAliased(renderer, [&renderer, &h] {
    HalFile file;
    if (!Storage.openFileForRead("SDK", DashboardSync::kDashboardPath, file)) return;
    Bitmap bitmap(file);
    if (bitmap.parseHeaders() == BmpReaderError::Ok) renderer.drawBitmap(bitmap, h.dashX, h.dashY, h.dashW, h.dashH);
  });
}
// Runs on the loop task with the RenderLock held (from callEvent), so the popup
// can go straight to the frame buffer.
m3ApiRawFunction(xSync) {
  m3ApiReturnType(int32_t) Rt& h_ = host(runtime);
  if (h_.inRender) m3ApiReturn(0);
  GUI.drawPopup(*h_.renderer, tr(STR_DASHBOARD_UPDATING));
  h_.renderer->displayBuffer();
  const auto r = DashboardSync::run(true);
  DashboardSync::stopRadio();
  LOG_INF("SDK", "sync: wifi=%d dashboard=%d, heap %u", r.wifi, r.dashboard,
          static_cast<unsigned>(HalMemory::getInternalHeap().freeBytes));
  m3ApiReturn(r.dashboard ? 1 : 0);
}

struct Import {
  const char* name;
  const char* sig;
  M3RawCall fn;
};
constexpr Import IMPORTS[] = {
    {"x4_fill", "v(iiiii)", &xFill},
    {"x4_rfill", "v(iiiiii)", &xRfill},
    {"x4_frame", "v(iiiiii)", &xFrame},
    {"x4_line", "v(iiiiii)", &xLine},
    {"x4_text", "v(ii*ii)", &xText},
    {"x4_textw", "i(*ii)", &xTextW},
    {"x4_lineh", "i(i)", &xLineH},
    {"x4_textbox", "i(iiii*ii)", &xTextBox},
    {"x4_image", "i(*iiii)", &xImage},
    {"x4_refresh", "v(i)", &xRefresh},
    {"x4_title", "v(*)", &xTitle},
    {"x4_hints", "v(****)", &xHints},
    {"x4_width", "i()", &xWidth},
    {"x4_top", "i()", &xTop},
    {"x4_bottom", "i()", &xBottom},
    {"x4_save", "i(*i)", &xSave},
    {"x4_load", "i(*i)", &xLoad},
    {"x4_fsize", "i(*)", &xFsize},
    {"x4_read", "i(*i*i)", &xRead},
    {"x4_rnd", "i()", &xRnd},
    {"x4_millis", "i()", &xMillis},
    {"x4_lang", "i()", &xLang},
    {"x4_log", "v(*)", &xLog},
    {"x4_list", "i(i*i)", &xList},
    {"x4_stats", "i(*)", &xStats},
    {"x4_dashboard", "i(iiii)", &xDashboard},
    {"x4_sync", "i()", &xSync},
    {"x4_rows", "i(**i)", &xRows},
    {"x4_image2", "i(*iiiii)", &xImage2},
};

}  // namespace

WasmAppActivity::WasmAppActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const AppManifest& manifest)
    : Activity("WasmApp", renderer, mappedInput), manifest(manifest) {}

WasmAppActivity::~WasmAppActivity() = default;

// Calls an export with the gas budget refilled; a trap or exhausted budget
// stops the app and shows why.
static bool callApp(Rt& h, IM3Function f, const char* what, const int32_t* arg, int32_t* ret) {
  if (!f) return true;  // optional export
  if (m3_SetResourceLimit(h.runtime, c_m3Limit_GasUnits, GAS_PER_CALL)) {
    snprintf(h.error, sizeof(h.error), "no gas metering");
    return false;
  }
  const M3Result r = arg ? m3_CallV(f, *arg) : m3_CallV(f);
  if (!r && ret) m3_GetResultsV(f, ret);
  if (r) {
    snprintf(h.error, sizeof(h.error), "%s: %s", what, r);
    LOG_ERR("SDK", "%s %s", h.manifest->id, h.error);
    return false;
  }
  return true;
}

bool WasmAppActivity::start() {
  Rt& h = *rt;
  char path[112];
  if (!app_path::resolve(manifest.dir, manifest.entry, path, sizeof(path))) return false;
  size_t len = 0;
  {
    HalFile f;
    if (!Storage.exists(path) || !Storage.openFileForRead("SDK", path, f)) {
      snprintf(h.error, sizeof(h.error), "missing %s", manifest.entry);
      return false;
    }
    len = f.fileSize();
    if (len == 0 || len > MAX_MODULE_BYTES) {
      snprintf(h.error, sizeof(h.error), "module too large");
      return false;
    }
    h.bytes = makeUniqueNoThrow<uint8_t[]>(len);
    if (!h.bytes || f.read(h.bytes.get(), len) != static_cast<int>(len)) return false;
  }
  h.env = m3_NewEnvironment();
  h.runtime = h.env ? m3_NewRuntime(h.env, WASM_STACK_BYTES, &h) : nullptr;
  if (!h.runtime) return false;
  m3_x4_SetMemoryCap(h.runtime, manifest.memoryKb * 1024U);
  // Arm gas before anything compiles: only bodies compiled afterwards are metered,
  // and m3_FindFunction compiles.
  if (m3_SetResourceLimit(h.runtime, c_m3Limit_GasUnits, GAS_PER_CALL)) {
    snprintf(h.error, sizeof(h.error), "no gas metering");
    return false;
  }
  IM3Module module;
  M3Result r = m3_ParseModule(h.env, &module, h.bytes.get(), len);
  if (r) {
    snprintf(h.error, sizeof(h.error), "parse: %s", r);
    return false;
  }
  r = m3_LoadModule(h.runtime, module);
  if (r) {
    snprintf(h.error, sizeof(h.error), "load: %s", r);
    return false;
  }
  for (const auto& imp : IMPORTS) {
    r = m3_LinkRawFunction(module, "x4", imp.name, imp.sig, imp.fn);
    if (r && r != m3Err_functionLookupFailed) {
      snprintf(h.error, sizeof(h.error), "%s: %s", imp.name, r);
      return false;
    }
  }
  if (m3_FindFunction(&h.render, h.runtime, "render") || m3_FindFunction(&h.event, h.runtime, "event")) {
    snprintf(h.error, sizeof(h.error), "must export render and event");
    return false;
  }
  // A reactor module (zig/clang -mexec-model=reactor) sets up its C runtime here.
  IM3Function initialize = nullptr;
  if (!m3_FindFunction(&initialize, h.runtime, "_initialize") &&
      !callApp(h, initialize, "_initialize", nullptr, nullptr)) {
    return false;
  }
  if (m3_FindFunction(&h.init, h.runtime, "init")) h.init = nullptr;
  if (m3_FindFunction(&h.exit, h.runtime, "exit")) h.exit = nullptr;
  return callApp(h, h.init, "init", nullptr, nullptr);
}

void WasmAppActivity::onEnter() {
  Activity::onEnter();
  const size_t heapBefore = HalMemory::getInternalHeap().freeBytes;
  rt = makeUniqueNoThrow<Runtime>();
  if (rt) {
    const auto& metrics = UITheme::getInstance().getMetrics();
    rt->manifest = &manifest;
    rt->renderer = &renderer;
    rt->top = metrics.topPadding + metrics.headerHeight + 12;
    rt->bottom = renderer.getScreenHeight() - metrics.buttonHintsHeight - 12;
    snprintf(rt->title, sizeof(rt->title), "%s", manifest.name());
    RenderLock lock;
    ok = start();
  }
  LOG_INF("SDK", "start %s: %s, heap %u -> %u", manifest.id, ok ? "ok" : (rt ? rt->error : "OOM"),
          static_cast<unsigned>(heapBefore), static_cast<unsigned>(HalMemory::getInternalHeap().freeBytes));
  requestUpdate();
}

void WasmAppActivity::onExit() {
  // ActivityManager already holds the RenderLock here.
  if (ok) callApp(*rt, rt->exit, "exit", nullptr, nullptr);
  rt.reset();
  LOG_INF("SDK", "exit %s, heap %u", manifest.id, static_cast<unsigned>(HalMemory::getInternalHeap().freeBytes));
  Activity::onExit();
}

bool WasmAppActivity::callEvent(const int event) {
  RenderLock lock;
  int32_t redraw = 0;
  const int32_t arg = event;
  if (!callApp(*rt, rt->event, "event", &arg, &redraw)) {
    ok = false;
    return true;  // show the error
  }
  return redraw != 0;
}

void WasmAppActivity::loop() {
  using Button = MappedInputManager::Button;
  if (mappedInput.wasReleased(Button::Back)) {
    finish();
    return;
  }
  if (!ok) return;
  // Events: 1 Confirm, 2 Left, 3 Right, 4 Up, 5 Down, 6 Confirm held.
  constexpr unsigned long HOLD_MS = 600;
  int event = 0;
  if (mappedInput.wasLongPressed(Button::Confirm, HOLD_MS)) {
    event = 6;
  } else if (mappedInput.wasReleased(Button::Confirm)) {
    event = 1;
  } else if (mappedInput.wasReleased(Button::Left)) {
    event = 2;
  } else if (mappedInput.wasReleased(Button::Right)) {
    event = 3;
  } else if (mappedInput.wasReleased(Button::Up)) {
    event = 4;
  } else if (mappedInput.wasReleased(Button::Down)) {
    event = 5;
  }
  if (event && callEvent(event)) requestUpdate();
}

void WasmAppActivity::render(RenderLock&&) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const int pageW = renderer.getScreenWidth();
  int mode = 0;
  if (ok) {
    rt->refreshMode = 0;
    rt->dashW = 0;
    rt->inRender = true;
    ok = callApp(*rt, rt->render, "render", nullptr, nullptr);
    rt->inRender = false;
    mode = rt->refreshMode;
  }
  if (!ok) {
    // Stopped or never started: say why instead of a blank screen.
    const int y = renderer.getScreenHeight() / 2 - 40;
    renderer.drawCenteredText(UI_12_FONT_ID, y, tr(STR_SDK_APP_STOPPED), true, EpdFontFamily::BOLD);
    const auto lines = app_text::wrap(renderer, UI_10_FONT_ID, rt ? rt->error : "OOM", pageW - 48, 4);
    int ly = y + 36;
    for (const auto& l : lines) {
      renderer.drawCenteredText(UI_10_FONT_ID, ly, l.c_str());
      ly += renderer.getLineHeight(UI_10_FONT_ID);
    }
  }
  // After the app's render(), so a title it set with x4_title shows in the same frame.
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageW, metrics.headerHeight}, rt ? rt->title : manifest.name(),
                 nullptr);
  const char* labels[4] = {tr(STR_BACK), "", "", ""};
  if (ok && rt) {
    for (int i = 1; i < 4; ++i) labels[i] = rt->hints[i];
    if (rt->hints[0][0]) labels[0] = rt->hints[0];
  }
  const auto mapped = mappedInput.mapLabels(labels[0], labels[1], labels[2], labels[3]);
  GUI.drawButtonHints(renderer, mapped.btn1, mapped.btn2, mapped.btn3, mapped.btn4);
  if (mode == 1 || ++fastRefreshes >= FAST_REFRESHES_PER_HALF) {
    fastRefreshes = 0;
    renderer.displayBuffer(HalDisplay::HALF_REFRESH);
  } else {
    renderer.displayBuffer();
  }
  if (ok && rt && rt->dashW > 0) dashboardGreyPass(renderer, *rt);
}
