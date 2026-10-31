#include "EpdFontFamily.h"

#include <cstddef>

const EpdFont* EpdFontFamily::getFont(const Style style) const {
  // Extract font style bits; render-time overlay bits do not affect font selection.
  const bool hasBold = (style & BOLD) != 0;
  const bool hasItalic = (style & ITALIC) != 0;

  if (hasBold && hasItalic) {
    if (boldItalic) return boldItalic;
    if (bold) return bold;
    if (italic) return italic;
  } else if (hasBold && bold) {
    return bold;
  } else if (hasItalic && italic) {
    return italic;
  }

  return regular;
}

void EpdFontFamily::getTextDimensions(const char* string, int* w, int* h, const Style style) const {
  getFont(style)->getTextDimensions(string, w, h);
}

const EpdFontData* EpdFontFamily::getData(const Style style) const { return getFont(style)->data; }

const EpdGlyph* EpdFontFamily::getGlyph(const uint32_t cp, const Style style) const {
  return getFont(style)->getGlyph(cp);
}

namespace {
// PERSONAL-HOOK(thai-fallback)
constexpr bool isThaiOrThaiPua(const uint32_t cp) {
  return (cp >= 0x0E01 && cp <= 0x0E5B) || (cp >= 0xF700 && cp <= 0xF71A);
}
constexpr uint8_t kFallbackSizes[] = {12, 14, 16, 18};
const EpdFontFamily* gThaiFallbacks[4] = {nullptr, nullptr, nullptr, nullptr};
}  // namespace

bool EpdFontFamily::hasCodepoint(const uint32_t cp, const Style style) const {
  if (getFont(style)->hasCodepoint(cp)) return true;
  // Thai and its C90 variants are drawable through the fallback (this is also
  // what makes GfxRenderer shape Thai for an SD font that has no Thai at all).
  return thaiFallback && isThaiOrThaiPua(cp) && thaiFallback->hasCodepoint(cp, style);
}

EpdFontFamily::GlyphSource EpdFontFamily::resolveGlyph(const uint32_t cp, const Style style) const {
  const EpdFont* font = getFont(style);
  // hasCodepoint() answers from RAM (SD fonts keep a coverage index), so a
  // Thai glyph the SD font lacks never costs an SD read here.
  if (thaiFallback && isThaiOrThaiPua(cp) && !font->hasCodepoint(cp)) {
    const GlyphSource borrowed = thaiFallback->resolveGlyph(cp, style);
    if (borrowed.glyph) return borrowed;
  }
  return {font->getGlyph(cp), font->data};
}

EpdFontFamily::GlyphSource EpdFontFamily::resolveGlyph(const uint32_t cp, EpdGlyph& solidFallback,
                                                       const Style style) const {
  // Solid symbols first: getGlyph() falls back to U+FFFD and would never return null. They are never Thai.
  if (syntheticGlyph::isSolid(cp)) {
    const EpdFont* font = getFont(style);
    return {font->getGlyphMetrics(cp, solidFallback), font->data};
  }
  return resolveGlyph(cp, style);
}

void EpdFontFamily::registerBuiltinThaiFallback(const uint8_t pointSize, const EpdFontFamily* family) {
  for (size_t i = 0; i < sizeof(kFallbackSizes); ++i) {
    if (kFallbackSizes[i] == pointSize) gThaiFallbacks[i] = family;
  }
}

const EpdFontFamily* EpdFontFamily::builtinThaiFallback(const uint8_t pointSize) {
  const EpdFontFamily* best = nullptr;
  int bestDelta = 1000;
  for (size_t i = 0; i < sizeof(kFallbackSizes); ++i) {
    if (!gThaiFallbacks[i]) continue;
    const int delta = pointSize > kFallbackSizes[i] ? pointSize - kFallbackSizes[i] : kFallbackSizes[i] - pointSize;
    if (delta < bestDelta) {
      bestDelta = delta;
      best = gThaiFallbacks[i];
    }
  }
  return best;
}

int8_t EpdFontFamily::getKerning(const uint32_t leftCp, const uint32_t rightCp, const Style style) const {
  return getFont(style)->getKerning(leftCp, rightCp);
}

uint32_t EpdFontFamily::applyLigatures(const uint32_t cp, const char*& text, const Style style) const {
  return getFont(style)->applyLigatures(cp, text);
}
