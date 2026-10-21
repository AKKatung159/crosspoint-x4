#include "AppText.h"

#include <GfxRenderer.h>
#include <ThaiBrk.h>

#include <algorithm>
#include <string_view>

namespace app_text {
namespace {

struct Break {
  size_t pos;   // line ends here
  size_t skip;  // bytes dropped before the next line (1 for a space)
};

// Appends the lines of one paragraph (no '\n'); returns false once maxLines is reached.
bool wrapParagraph(const GfxRenderer& r, const int fontId, const std::string_view para, const int maxWidth,
                   const int maxLines, const EpdFontFamily::Style style, std::vector<std::string>& lines) {
  std::vector<Break> breaks = {};
  for (const size_t pos : ThaiBrk::wordBreakByteOffsets(para)) breaks.push_back({pos, 0});
  for (size_t i = 0; i < para.size(); ++i) {
    if (para[i] == ' ') breaks.push_back({i, 1});
  }
  breaks.push_back({para.size(), 0});
  std::sort(breaks.begin(), breaks.end(), [](const Break& a, const Break& b) {
    return a.pos != b.pos ? a.pos < b.pos : a.skip < b.skip;  // at a tie, the space break wins (taken last)
  });

  const auto fits = [&](const size_t from, const size_t to) {
    return r.getTextWidth(fontId, std::string(para.substr(from, to - from)).c_str(), style) <= maxWidth;
  };
  size_t start = 0;
  for (size_t i = 0; i < breaks.size() && start < para.size();) {
    if (static_cast<int>(lines.size()) == maxLines - 1) {
      // Last allowed line: the rest, truncated if it does not fit.
      lines.push_back(r.truncatedText(fontId, std::string(para.substr(start)).c_str(), maxWidth, style));
      return false;
    }
    size_t lastFit = SIZE_MAX;
    while (i < breaks.size() && (breaks[i].pos <= start || fits(start, breaks[i].pos))) {
      if (breaks[i].pos > start) lastFit = i;
      ++i;
    }
    if (lastFit == SIZE_MAX) {
      // One segment wider than the line: cut it with an ellipsis.
      const size_t end = i < breaks.size() ? breaks[i].pos : para.size();
      lines.push_back(r.truncatedText(fontId, std::string(para.substr(start, end - start)).c_str(), maxWidth, style));
      start = end + (i < breaks.size() ? breaks[i].skip : 0);
      ++i;
    } else {
      lines.emplace_back(para.substr(start, breaks[lastFit].pos - start));
      start = breaks[lastFit].pos + breaks[lastFit].skip;
      i = lastFit + 1;
    }
  }
  return static_cast<int>(lines.size()) < maxLines;
}

}  // namespace

std::vector<std::string> wrap(const GfxRenderer& renderer, const int fontId, const char* text, const int maxWidth,
                              const int maxLines, const EpdFontFamily::Style style) {
  std::vector<std::string> lines;
  std::string_view rest(text);
  while (static_cast<int>(lines.size()) < maxLines) {
    const size_t nl = rest.find('\n');
    const std::string_view para = rest.substr(0, nl);
    if (para.empty()) {
      lines.emplace_back();
    } else if (!wrapParagraph(renderer, fontId, para, maxWidth, maxLines, style, lines)) {
      break;
    }
    if (nl == std::string_view::npos) break;
    rest.remove_prefix(nl + 1);
  }
  return lines;
}

}  // namespace app_text
