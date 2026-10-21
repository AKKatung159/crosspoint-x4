#include "CoverTheme.h"

#include <Bitmap.h>
#include <GfxRenderer.h>
#include <HalPowerManager.h>
#include <HalStorage.h>
#include <I18n.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "HomeSummary.h"
#include "RecentBooksStore.h"
#include "components/UITheme.h"
#include "components/icons/personal/homeIcons.h"
#include "fontIds.h"

namespace {

constexpr int kMenuIcon = 44;
constexpr int kHintIcon = 28;

// drawIcon() only paints ink; this paints an icon in either colour so a
// selected icon can be knocked out of its black disc. Same bit layout as
// GfxRenderer::drawIcon (1bpp, MSB first, 0 = ink, rotated).
void drawIconIn(const GfxRenderer& r, const uint8_t* bitmap, const int x, const int y, const int size,
                const bool black) {
  const int rowBytes = (size + 7) / 8;
  for (int row = 0; row < size; row++) {
    for (int col = 0; col < size; col++) {
      const uint8_t byte = bitmap[row * rowBytes + (col >> 3)];
      if (((byte >> (7 - (col & 7))) & 1) == 0) r.drawPixel(x + (size - 1 - row), y + col, black);
    }
  }
}

const uint8_t* menuIcon(const UIIcon icon, const bool small) {
  switch (icon) {
    case Library:
      return small ? HomeLibrary28 : HomeLibrary44;
    case Folder:
      return small ? HomeFolder28 : HomeFolder44;
    case Transfer:
      return small ? HomeTransfer28 : HomeTransfer44;
    case Settings:
      return small ? HomeSettings28 : HomeSettings44;
    case Blocks:  // OPDS catalogue
      return small ? HomeCatalog28 : HomeCatalog44;
    case Stats:  // the Apps tile (Pocket Hub)
      return small ? HomeApps28 : HomeApps44;
    case Book:
      return small ? HomeResume28 : HomeResume44;
    default:
      return nullptr;
  }
}

// Short tile labels: the menu's own strings ("Browse Files") do not fit a quarter of the screen.
const char* tileLabel(const UIIcon icon) {
  switch (icon) {
    case Folder:
      return tr(STR_TILE_FILES);
    case Library:
      return tr(STR_TILE_LIBRARY);
    case Transfer:
      return tr(STR_TILE_TRANSFER);
    case Settings:
      return tr(STR_TILE_SETTINGS);
    case Blocks:
      return "OPDS";
    case Stats:  // the Apps tile (Pocket Hub)
      return tr(STR_TILE_APPS);
    default:
      return nullptr;
  }
}

// Hint labels Home passes, mapped to the icon family (minimal theme only).
const uint8_t* hintIcon(const char* label) {
  if (!label || !*label) return nullptr;
  if (std::strcmp(label, tr(STR_RESUME)) == 0) return HomeResume28;
  if (std::strcmp(label, tr(STR_SELECT)) == 0) return HomeSelect28;
  if (std::strcmp(label, tr(STR_DIR_UP)) == 0 || std::strcmp(label, tr(STR_DIR_LEFT)) == 0) return HomePrev28;
  if (std::strcmp(label, tr(STR_DIR_DOWN)) == 0 || std::strcmp(label, tr(STR_DIR_RIGHT)) == 0) return HomeNext28;
  return nullptr;
}

// "<cache>/thumb_[HEIGHT].bmp" -> "<cache>/thumbg_<height>.bmp" (the 2-bit thumb, EPUB only).
std::string greyThumbPath(std::string templatePath, const int thumbHeight) {
  const size_t pos = templatePath.find("thumb_[HEIGHT]");
  if (pos == std::string::npos) return {};
  return templatePath.replace(pos, 14, "thumbg_" + std::to_string(thumbHeight));
}

bool drawThumb(const GfxRenderer& renderer, const std::string& path, const Rect slot) {
  HalFile file;
  if (path.empty() || !Storage.exists(path.c_str()) || !Storage.openFileForRead("HOME", path, file)) return false;
  Bitmap bitmap(file);
  const bool drawn =
      bitmap.parseHeaders() == BmpReaderError::Ok && BaseTheme::drawCoverThumbFill(renderer, bitmap, slot);
  file.close();
  return drawn;
}

// Draws the cover centred in `art` at its own shape (thumbs keep the cover's
// aspect, often wider than 0.6), or the placeholder in `slot`. Returns the grey
// thumb's path when that is what was drawn (the grey pass redraws it), else "".
std::string drawCover(const GfxRenderer& renderer, const RecentBook& book, const Rect slot, const Rect art,
                      const int thumbHeight) {
  if (!book.coverBmpPath.empty()) {
    std::string grey = greyThumbPath(book.coverBmpPath, thumbHeight);
    if (drawThumb(renderer, grey, art)) return grey;
    if (drawThumb(renderer, UITheme::getCoverThumbPath(book.coverBmpPath, thumbHeight), art)) return {};
  }
  // No thumbnail yet: a grey field with the book shape, never an empty hole.
  renderer.fillRectDither(slot.x, slot.y, slot.width, slot.height, Color::LightGray);
  drawIconIn(renderer, HomeResume44, slot.x + (slot.width - kMenuIcon) / 2, slot.y + (slot.height - kMenuIcon) / 2,
             kMenuIcon, true);
  return {};
}

// Filled track (dithered grey) with a solid fill: no hairlines.
void drawProgress(const GfxRenderer& renderer, const Rect r, const int percent) {
  renderer.fillRectDither(r.x, r.y, r.width, r.height, Color::LightGray);
  const int w = r.width * std::clamp(percent, 0, 100) / 100;
  if (w > 0) renderer.fillRect(r.x, r.y, w, r.height, true);
}

// One small solid dot per recent book, the shown one filled larger.
void drawBookDots(const GfxRenderer& renderer, const int centerX, const int y, const int count, const int current) {
  if (count < 2) return;
  constexpr int gap = 14;
  const int x0 = centerX - (count - 1) * gap / 2;
  for (int i = 0; i < count; i++) {
    const int s = i == current ? 8 : 4;
    renderer.fillRoundedRect(x0 + i * gap - s / 2, y - s / 2, s, s, s / 2,
                             i == current ? Color::Black : Color::DarkGray);
  }
}

}  // namespace

void CoverTheme::homeCoverGreyPass(const GfxRenderer& renderer) const {
  drawThumb(renderer, greyCoverPath, greyCoverSlot);
}

int CoverTheme::homeCoverThumbHeight(const GfxRenderer&) const {
  return minimal ? CoverThemeMetrics::minimal.homeCoverHeight : CoverThemeMetrics::cover.homeCoverHeight;
}

void CoverTheme::drawHeader(const GfxRenderer& renderer, const Rect rect, const char* title, const char* subtitle,
                            const bool backButton) const {
  // The minimal home has no words: only a solid battery glyph, top right.
  if (!minimal || title != nullptr || subtitle != nullptr || backButton) {
    LyraTheme::drawHeader(renderer, rect, title, subtitle, backButton);
    return;
  }
  const int pct = std::clamp<int>(powerManager.getBatteryPercentage(), 0, 100);
  const int w = 26, h = 14;
  const int x = renderer.getScreenWidth() - 20 - w - 4, y = rect.y + 6;
  renderer.fillRect(x, y, w, h, true);
  renderer.fillRect(x + w, y + 4, 4, h - 8, true);
  const int empty = (w - 6) * (100 - pct) / 100;  // knock out the spent part
  if (empty > 0) renderer.fillRect(x + 3 + (w - 6) - empty, y + 3, empty, h - 6, false);
}

void CoverTheme::drawRecentBookCover(GfxRenderer& renderer, const Rect rect, const std::vector<RecentBook>& recentBooks,
                                     const int selectorIndex, bool& coverRendered, bool& coverBufferStored,
                                     bool& bufferRestored, std::function<bool()>) const {
  onHome = true;
  greyCoverPath.clear();
  coverRendered = false;  // the cover follows the selection; redraw it each pass
  coverBufferStored = false;
  bufferRestored = false;
  if (recentBooks.empty()) {
    drawEmptyRecents(renderer, rect);
    return;
  }
  const int count = std::min<int>(
      recentBooks.size(), (minimal ? CoverThemeMetrics::minimal : CoverThemeMetrics::cover).homeRecentBooksCount);
  if (selectorIndex >= 0 && selectorIndex < count) shownBook = selectorIndex;
  shownBook = std::clamp(shownBook, 0, count - 1);
  const RecentBook& book = recentBooks[shownBook];

  const int thumbH = homeCoverThumbHeight(renderer);
  const int coverH = thumbH;
  const int coverW = thumbH * 3 / 5;  // thumbnails are generated 0.6 : 1
  const int cx = rect.x + rect.width / 2;
  const Rect slot{cx - coverW / 2, rect.y, coverW, coverH};
  const Rect art{rect.x + 16, rect.y, rect.width - 32, coverH};
  greyCoverPath = drawCover(renderer, book, slot, art, thumbH);
  greyCoverSlot = art;

  int percent = 0;
  std::string chapter;
  const bool haveSummary = HomeSummary::read(HomeSummary::cacheDirFromThumbPath(book.coverBmpPath), percent, chapter);

  if (minimal) {
    // Cover, a solid progress bar of the same width, the book dots. No words.
    drawProgress(renderer, Rect{slot.x, slot.y + coverH + 18, coverW, 8}, haveSummary ? percent : 0);
    drawBookDots(renderer, cx, slot.y + coverH + 44, count, shownBook);
    return;
  }

  // Cover: title, author · chapter, progress bar, "read N%".
  int y = slot.y + coverH + 10;
  const int textW = rect.width - 2 * 24;
  const std::string title = renderer.truncatedText(NOTOSANS_18_FONT_ID, book.title.c_str(), textW, EpdFontFamily::BOLD);
  renderer.drawCenteredText(NOTOSANS_18_FONT_ID, y, title.c_str(), true, EpdFontFamily::BOLD);
  y += renderer.getLineHeight(NOTOSANS_18_FONT_ID);
  std::string meta = book.author;
  if (!chapter.empty()) meta += (meta.empty() ? "" : " · ") + chapter;
  if (!meta.empty()) {
    const std::string line = renderer.truncatedText(UI_10_FONT_ID, meta.c_str(), textW);
    renderer.drawCenteredText(UI_10_FONT_ID, y, line.c_str());
  }
  y += renderer.getLineHeight(UI_10_FONT_ID) + 4;
  drawProgress(renderer, Rect{slot.x, y, coverW, 10}, haveSummary ? percent : 0);
  y += 14;
  if (haveSummary) {
    const std::string read = std::string(tr(STR_HOME_READ_PERCENT)) + " " + std::to_string(percent) + "%";
    renderer.drawCenteredText(UI_10_FONT_ID, y, read.c_str());
  }
  drawBookDots(renderer, cx, y + renderer.getLineHeight(UI_10_FONT_ID) + 4, count, shownBook);
}

void CoverTheme::drawButtonMenu(GfxRenderer& renderer, const Rect rect, const int buttonCount, const int selectedIndex,
                                const std::function<std::string(int index)>& buttonLabel,
                                const std::function<UIIcon(int index)>& rowIcon) const {
  if (!onHome || buttonCount <= 0) {
    LyraTheme::drawButtonMenu(renderer, rect, buttonCount, selectedIndex, buttonLabel, rowIcon);
    return;
  }
  // Six tiles (with OPDS and Stats) need the tighter spacing for Thai labels.
  const int side = buttonCount > 5 ? 10 : 24;
  const int gap = buttonCount > 5 ? 4 : 12;
  const int tileW = (rect.width - 2 * side - (buttonCount - 1) * gap) / buttonCount;

  for (int i = 0; i < buttonCount; i++) {
    const bool selected = i == selectedIndex;
    const uint8_t* icon = rowIcon ? menuIcon(rowIcon(i), false) : nullptr;
    const int x = rect.x + side + i * (tileW + gap);
    if (minimal) {
      // Icons only; the selected one sits knocked out of a solid disc.
      const int cx = x + tileW / 2, cy = rect.y + 46;
      if (selected) renderer.fillRoundedRect(cx - 38, cy - 38, 76, 76, 38, Color::Black);
      if (icon) drawIconIn(renderer, icon, cx - kMenuIcon / 2, cy - kMenuIcon / 2, kMenuIcon, !selected);
      continue;
    }
    // Tiles: a light grey field, the selected one solid black; icon over label.
    const int h = 84;
    renderer.fillRoundedRect(x, rect.y, tileW, h, 14, selected ? Color::Black : Color::LightGray);
    if (icon) drawIconIn(renderer, icon, x + (tileW - kMenuIcon) / 2, rect.y + 8, kMenuIcon, !selected);
    const char* shortLabel = rowIcon ? tileLabel(rowIcon(i)) : nullptr;
    const std::string label =
        renderer.truncatedText(UI_10_FONT_ID, shortLabel ? shortLabel : buttonLabel(i).c_str(), tileW - 6);
    const int lw = renderer.getTextWidth(UI_10_FONT_ID, label.c_str());
    renderer.drawText(UI_10_FONT_ID, x + (tileW - lw) / 2, rect.y + 54, label.c_str(), !selected);
  }
}

void CoverTheme::drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                                 const char* btn4) const {
  const bool homePass = onHome;
  onHome = false;  // the pass ends with the hints
  if (!minimal || !homePass) {
    LyraTheme::drawButtonHints(renderer, btn1, btn2, btn3, btn4);
    return;
  }
  // Same positions as the text hints, but bare icons of the home family.
  const char* labels[4] = {btn1, btn2, btn3, btn4};
  const int pageW = renderer.getScreenWidth();
  const int y = renderer.getScreenHeight() - kHintIcon - 10;
  for (int i = 0; i < 4; i++) {
    const uint8_t* icon = hintIcon(labels[i]);
    if (!icon) continue;
    const int cx = pageW * (2 * i + 1) / 8;
    drawIconIn(renderer, icon, cx - kHintIcon / 2, y, kHintIcon, true);
  }
}
