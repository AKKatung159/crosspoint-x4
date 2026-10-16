#pragma once

#include <cstdint>
#include <string>
#include <string_view>

/// Contextual glyph selection for Thai, following the WTT 2.0 rules described in
/// https://linux.thai.net/~thep/th-otf/shaping.html and the de-facto C90 PUA
/// convention (U+F700-F71A) for pre-composed variant glyphs:
///
///   - SARA AM (U+0E33) decomposes to NIKHAHIT + SARA AA; a preceding tone
///     mark is reordered after the NIKHAHIT.
///   - Upper vowels / NIKHAHIT / MAITAIKHU shift left over ascender
///     consonants (ป ฝ ฟ ฬ).
///   - Tone marks keep their consonant tier with no upper vowel and take the
///     second (high) tier above SARA I/II/UE/UEE etc.; ascender consonants
///     additionally select left-shifted variants.
///   - YO YING (ญ) and THO THAN (ฐ) lose their descenders under below vowels;
///     below vowels drop under ฎ and ฏ.
///
/// The firmware draws glyph runs linearly (Thai marks are zero-advance glyphs
/// positioned by their font-native bearings), so shaping is a pure codepoint
/// substitution. It is applied at draw/measure time by GfxRenderer, only for
/// fonts that carry the C90 PUA glyphs; book text and section caches keep the
/// original Unicode. Shaping is idempotent (shaping shaped text is a no-op).
///
/// Ported from JTIAPBNAI/CrosspointTH (MIT), lib/ThaiShaping.
namespace ThaiShaping {

/// A codepoint every C90-capable font has; used to probe font support.
constexpr uint32_t PUA_PROBE = 0xF70A;

/// Gap, in pixels, kept between an upper vowel and a high-tier tone mark.
constexpr int kHighToneGapPx = 1;

/// True for the C90 high-tier tone marks (above SARA I/II/UE/UEE etc.).
constexpr bool isHighTone(const uint32_t cp) { return cp >= 0xF705 && cp <= 0xF70E; }

/// Vertical correction, in pixels (positive = down), for a high-tier tone mark
/// so it sits exactly kHighToneGapPx above the font's upper vowels. Rasterized
/// at e-ink sizes the .small/high forms land anywhere from touching the vowel
/// to 2 px above it, so a fixed offset either collides or floats; measure
/// instead. `toneTop`/`toneHeight` describe the tone glyph and `vowelTop` the
/// tallest of SARA II/UE/UEE, all in the font's glyph units (top = pixels above baseline).
/// Zero for every other codepoint.
constexpr int highToneAdjustPx(const uint32_t cp, const int toneTop, const int toneHeight, const int vowelTop) {
  if (!isHighTone(cp)) return 0;
  const int gap = (toneTop - toneHeight) - vowelTop;
  return gap - kHighToneGapPx;
}

/// True if shapeUtf8() would change `text`. Allocation-free scan used to keep
/// the render path copy-free for Latin text and for already-shaped Thai.
bool needsShaping(std::string_view text);

/// Returns the shaped UTF-8 string. Codepoints outside the Thai block pass
/// through untouched; the output may be longer than the input (SARA AM).
std::string shapeUtf8(std::string_view in);

}  // namespace ThaiShaping
