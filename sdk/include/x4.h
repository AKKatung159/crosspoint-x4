// Pocket SDK v1 for the Xteink X4 (crosspoint-x4 firmware).
//
// An app is a WebAssembly module plus a manifest in /apps/<folder>/ on the SD
// card. It exports, with X4_EXPORT, init() (optional), int event(int),
// render() and exit() (optional);
// the firmware calls them on its UI thread and redraws when event() returns 1.
// Back always leaves the app (exit() runs first), so it is never delivered.
//
// The screen is 1-bit e-ink, portrait (x4_width() is 480 on the X4). The app
// draws between x4_top() and x4_bottom(); the firmware draws the title bar and
// the button hints. Every refresh repaints the whole panel (~0.3 s), so redraw
// only when something changed.
#ifndef X4_SDK_H
#define X4_SDK_H

#include <stdint.h>

#define X4_SDK_VERSION 2  // manifest "sdk": 2 for the v2 calls at the end
// Host-side tests define these empty and link their own x4_* functions.
#ifndef X4_IMPORT
#define X4_IMPORT(name) __attribute__((import_module("x4"), import_name(#name)))
#endif
#ifndef X4_EXPORT
#define X4_EXPORT(name) __attribute__((export_name(#name)))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// event() codes
enum { X4_CONFIRM = 1, X4_LEFT = 2, X4_RIGHT = 3, X4_UP = 4, X4_DOWN = 5, X4_CONFIRM_HOLD = 6 };
// colors (1 and 2 are dithered greys)
enum { X4_WHITE = 0, X4_LIGHT = 1, X4_DARK = 2, X4_BLACK = 3 };
// fonts
enum { X4_SMALL = 0, X4_UI10 = 1, X4_UI12 = 2, X4_SANS16 = 3, X4_SANS18 = 4 };
// text flags
enum { X4_BOLD = 1, X4_INVERT = 2, X4_CENTER = 4 };  // X4_CENTER: x is the centre
// x4_refresh modes
enum { X4_FAST = 0, X4_HALF = 1 };  // HALF clears ghosting, takes longer

// Drawing: only inside render().
X4_IMPORT(x4_fill) void x4_fill(int x, int y, int w, int h, int color);
X4_IMPORT(x4_rfill) void x4_rfill(int x, int y, int w, int h, int radius, int color);
X4_IMPORT(x4_frame) void x4_frame(int x, int y, int w, int h, int thickness, int color);
X4_IMPORT(x4_line) void x4_line(int x1, int y1, int x2, int y2, int width, int color);
X4_IMPORT(x4_text) void x4_text(int x, int y, const char* utf8, int font, int flags);
// Wraps (Thai-aware) into a box `w` wide; returns the number of lines drawn.
X4_IMPORT(x4_textbox) int x4_textbox(int x, int y, int w, int max_lines, const char* utf8, int font, int flags);
// A BMP (1/2/4/8/24-bit) from the app's folder, fitted into max_w x max_h; 1 when drawn.
X4_IMPORT(x4_image) int x4_image(const char* path, int x, int y, int max_w, int max_h);
X4_IMPORT(x4_refresh) void x4_refresh(int mode);

// Measuring and screen geometry: any time.
X4_IMPORT(x4_textw) int x4_textw(const char* utf8, int font, int flags);
X4_IMPORT(x4_lineh) int x4_lineh(int font);
X4_IMPORT(x4_width) int x4_width(void);
X4_IMPORT(x4_top) int x4_top(void);
X4_IMPORT(x4_bottom) int x4_bottom(void);

// Chrome: the title bar text and the four front-button labels (Back, Confirm,
// Left, Right); pass 0 to leave one blank. They persist until changed.
X4_IMPORT(x4_title) void x4_title(const char* utf8);
X4_IMPORT(x4_hints) void x4_hints(const char* back, const char* confirm, const char* left, const char* right);

// State that survives exit and power loss, at most the manifest's "save" bytes.
// x4_load needs cap >= that size; returns the saved length or -1.
X4_IMPORT(x4_save) int x4_save(const void* data, int len);
X4_IMPORT(x4_load) int x4_load(void* data, int cap);

// Read-only files in the app's folder ("cards/fool.bmp"; no "..", no leading "/").
X4_IMPORT(x4_fsize) int x4_fsize(const char* path);
X4_IMPORT(x4_read) int x4_read(const char* path, int offset, void* out, int len);

// Misc.
X4_IMPORT(x4_rnd) uint32_t x4_rnd(void);          // hardware random
X4_IMPORT(x4_millis) uint32_t x4_millis(void);    // since boot
X4_IMPORT(x4_lang) int x4_lang(void);             // 0 English, 1 Thai
X4_IMPORT(x4_log) void x4_log(const char* utf8);  // to the serial log

// --- v2 ---------------------------------------------------------------------

// The n-th file (no folders, no dot files) in the app's folder, in card order,
// copied into out; its length, or -1 past the last one. Only names x4_read
// accepts are listed ([A-Za-z0-9._-], at most 63 bytes).
X4_IMPORT(x4_list) int x4_list(int n, char* out, int cap);

// Byte offsets of the rows of a TAB-separated file in the app's folder: lines
// with a TAB that do not start with '#' (after an optional UTF-8 BOM, which
// the offset skips). Fills up to max offsets; returns how many, or -1. The
// firmware scans the file, so a long one costs the app no gas.
X4_IMPORT(x4_rows) int x4_rows(const char* path, uint32_t* out, int max);

// x4_image with flags: X4_ROTATE180 draws a 1-bit BMP upside down in the same
// box (other BMPs are drawn upright). Only inside render(); 1 when drawn.
enum { X4_ROTATE180 = 1 };
X4_IMPORT(x4_image2) int x4_image2(const char* path, int x, int y, int max_w, int max_h, int flags);

// Reading statistics, as the firmware records them.
typedef struct {
  uint32_t total_sec, total_pages, sessions, finished;
  uint32_t streak;      // consecutive days with reading, ending today or yesterday
  int32_t clock_known;  // 0: the clock was never set, so week[] is empty
  struct {
    char key[12];  // "YYYY-MM-DD", "" if unknown
    uint32_t sec, pages;
  } week[7];             // oldest first, week[6] is today
  uint32_t last_sec;     // time in the most recent book
  int32_t last_percent;  // its progress, -1 if unknown
  char last_title[96];   // "" if none
} x4_stats_t;
// Fills out; sizeof(x4_stats_t), or -1 when nothing has been read yet.
X4_IMPORT(x4_stats) int x4_stats(x4_stats_t* out);

// The last dashboard image (Settings -> Display -> Dashboard URL), scaled down
// into max_w x max_h from (x, y); 1 when drawn. Only inside render().
X4_IMPORT(x4_dashboard) int x4_dashboard(int x, int y, int max_w, int max_h);
// Joins the last Wi-Fi network and fetches a new dashboard, showing a popup
// meanwhile (a few seconds); 1 on success. Not inside render().
X4_IMPORT(x4_sync) int x4_sync(void);

#ifdef __cplusplus
}
#endif
#endif
