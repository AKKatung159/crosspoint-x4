#pragma once

#include <EpdFontFamily.h>

#include <string>
#include <vector>

class GfxRenderer;

// PERSONAL: text layout for Pocket Hub apps.
namespace app_text {

// Wraps `text` into at most `maxLines` lines no wider than `maxWidth`. Breaks
// at spaces, at Thai word boundaries (ThaiBrk) and always at '\n'. A segment
// wider than a line, or text past maxLines, ends in an ellipsis.
std::vector<std::string> wrap(const GfxRenderer& renderer, int fontId, const char* text, int maxWidth, int maxLines,
                              EpdFontFamily::Style style = EpdFontFamily::REGULAR);

}  // namespace app_text
