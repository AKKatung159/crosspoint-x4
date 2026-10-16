#pragma once

#include "components/themes/lyra/LyraTheme.h"

// Personal home themes (this fork; designs in docs/themes/). Both are Lyra
// everywhere except the home screen, where the book cover is the one big
// thing:
//   Cover   - large cover, Thai title, author · chapter, progress bar and
//             percent, four icon tiles with labels.
//   Minimal - larger cover, a progress bar, four icons; no text at all
//             (battery and button hints are icons too).
// Books are walked with next/previous like the rest of the home list (books
// first, then the tiles); the cover shows the selected book, or the last one
// selected while the selection is on the tiles.

namespace CoverThemeMetrics {
constexpr ThemeMetrics cover = [] {
  ThemeMetrics v = LyraMetrics::values;
  v.homeTopPadding = 48;
  v.homeCoverHeight = 420;
  v.homeCoverTileHeight = 560;
  v.homeRecentBooksCount = 5;
  v.homeContinueReadingInMenu = false;
  v.homeMenuTopOffset = 12;
  return v;
}();
constexpr ThemeMetrics minimal = [] {
  ThemeMetrics v = cover;
  v.homeTopPadding = 40;
  v.homeCoverHeight = 520;
  v.homeCoverTileHeight = 576;
  return v;
}();
}  // namespace CoverThemeMetrics

class CoverTheme : public LyraTheme {
 public:
  explicit CoverTheme(bool minimal) : minimal(minimal) {}

  void drawHeader(const GfxRenderer& renderer, Rect rect, const char* title, const char* subtitle = nullptr,
                  bool backButton = true) const override;
  void drawRecentBookCover(GfxRenderer& renderer, Rect rect, const std::vector<RecentBook>& recentBooks,
                           const int selectorIndex, bool& coverRendered, bool& coverBufferStored, bool& bufferRestored,
                           std::function<bool()> storeCoverBuffer) const override;
  void drawButtonMenu(GfxRenderer& renderer, Rect rect, int buttonCount, int selectedIndex,
                      const std::function<std::string(int index)>& buttonLabel,
                      const std::function<UIIcon(int index)>& rowIcon) const override;
  void drawButtonHints(GfxRenderer& renderer, const char* btn1, const char* btn2, const char* btn3,
                       const char* btn4) const override;
  int homeCoverThumbHeight(const GfxRenderer& renderer) const override;
  bool homeGreyCovers() const override { return true; }
  bool homeCoverIsGrey() const override { return !greyCoverPath.empty(); }
  void homeCoverGreyPass(const GfxRenderer& renderer) const override;

 private:
  bool minimal;
  // Home renders header, cover, menu, then hints in one pass; the cover call
  // marks the pass so the hint bar (shared with every screen) knows it is on home.
  mutable bool onHome = false;
  mutable int shownBook = 0;
  // The grey thumb drawn on the last home pass and where, for the grey pass.
  mutable std::string greyCoverPath;
  mutable Rect greyCoverSlot{};
};
