#pragma once
#include "EpdFont.h"

class EpdFontFamily {
 public:
  // Bitmask of text style flags carried per-word through layout and serialized in page cache.
  // Bits 0-1 select the font variant (BOLD/ITALIC); bits 2-5 are decoration/positioning overlays
  // applied at render time without changing the underlying font. getFont() ignores all bits
  // above bit 1 so decorations compose freely with bold/italic (e.g. BOLD | UNDERLINE | SUP).
  enum Style : uint8_t {
    REGULAR = 0,
    BOLD = 1,
    ITALIC = 2,
    BOLD_ITALIC = 3,
    UNDERLINE = 4,       // drawn as a line below baseline by TextBlock::render()
    STRIKETHROUGH = 8,   // drawn as a line through midline by TextBlock::render()
    SUP = 16,            // superscript: glyph scaled 50%, raised ~40% of ascender
    SUB = 32,            // subscript: glyph scaled 50%, lowered ~25% of ascender
    RUBY_CONTINUE = 64,  // Group ruby follower marker (used internally by Epub layout)
    SMALL_CAPS = 128,    // lowercase letters drawn as uppercase glyphs at 3/4 scale
  };
  static constexpr uint8_t TEXT_DECORATION_MASK = static_cast<uint8_t>(UNDERLINE | STRIKETHROUGH);

  explicit EpdFontFamily(const EpdFont* regular, const EpdFont* bold = nullptr, const EpdFont* italic = nullptr,
                         const EpdFont* boldItalic = nullptr)
      : regular(regular), bold(bold), italic(italic), boldItalic(boldItalic) {}
  ~EpdFontFamily() = default;
  void getTextDimensions(const char* string, int* w, int* h, Style style = REGULAR) const;
  const EpdFontData* getData(Style style = REGULAR) const;
  const EpdGlyph* getGlyph(uint32_t cp, Style style = REGULAR) const;
  const EpdGlyph* getGlyphMetrics(uint32_t cp, EpdGlyph& solidFallback, Style style = REGULAR) const {
    return getFont(style)->getGlyphMetrics(cp, solidFallback);
  }

  // PERSONAL-HOOK(thai-fallback): SD-card fonts without Thai borrow Thai and the
  // C90 variant glyphs from a built-in family of the nearest point size.
  /// A glyph together with the font data that owns its bitmap (they must travel
  /// together: getGlyphBitmap() resolves the bitmap relative to that data).
  struct GlyphSource {
    const EpdGlyph* glyph;
    const EpdFontData* data;
  };
  /// Like getGlyph()/getData(), but a Thai or C90 PUA codepoint this family
  /// lacks comes from its Thai fallback family, when one is set.
  GlyphSource resolveGlyph(uint32_t cp, Style style = REGULAR) const;
  /// resolveGlyph() plus getGlyphMetrics()'s synthetic solid glyphs: a solid
  /// symbol the font lacks comes back as `solidFallback`, which has no bitmap.
  GlyphSource resolveGlyph(uint32_t cp, EpdGlyph& solidFallback, Style style = REGULAR) const;
  void setThaiFallback(const EpdFontFamily* family) { thaiFallback = family; }
  /// Built-in Thai-capable families by point size, registered once at boot.
  static void registerBuiltinThaiFallback(uint8_t pointSize, const EpdFontFamily* family);
  /// The registered family of `pointSize`, else the nearest size; nullptr if none.
  static const EpdFontFamily* builtinThaiFallback(uint8_t pointSize);
  /// Returns true if the resolved style's font can render `cp`: interval coverage
  /// (see EpdFont::hasCodepoint), plus Thai borrowed from the Thai fallback (PERSONAL-HOOK(thai-fallback)).
  bool hasCodepoint(uint32_t cp, Style style = REGULAR) const;
  int8_t getKerning(uint32_t leftCp, uint32_t rightCp, Style style = REGULAR) const;
  uint32_t applyLigatures(uint32_t cp, const char*& text, Style style = REGULAR) const;
  static constexpr bool hasTextDecoration(const Style style) {
    return (static_cast<uint8_t>(style) & TEXT_DECORATION_MASK) != 0;
  }

 private:
  const EpdFont* regular;
  const EpdFont* bold;
  const EpdFont* italic;
  const EpdFont* boldItalic;
  const EpdFontFamily* thaiFallback = nullptr;  // PERSONAL-HOOK(thai-fallback)

  const EpdFont* getFont(Style style) const;
};
