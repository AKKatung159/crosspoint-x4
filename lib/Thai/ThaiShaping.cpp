#include "ThaiShaping.h"

#include <Utf8.h>

// Ported from JTIAPBNAI/CrosspointTH v1.6.0-th.1.0 (MIT), lib/ThaiShaping.
// Changes: string_view input, needsShaping() fast path.

namespace ThaiShaping {
namespace {

constexpr uint32_t SARA_AM = 0x0E33;
constexpr uint32_t NIKHAHIT = 0x0E4D;
constexpr uint32_t SARA_AA = 0x0E32;

// Consonants whose ascender stem collides with upper marks: ป ฝ ฟ ฬ.
constexpr bool isAscenderBase(const uint32_t cp) {
  return cp == 0x0E1B || cp == 0x0E1D || cp == 0x0E1F || cp == 0x0E2C;
}

// Consonants whose descender collides with below vowels: ฎ ฏ.
constexpr bool isDescenderBase(const uint32_t cp) { return cp == 0x0E0E || cp == 0x0E0F; }

// ญ ฐ: drop the descender under a below vowel.
constexpr bool isDescenderlessCandidate(const uint32_t cp) { return cp == 0x0E0D || cp == 0x0E10; }

constexpr bool isBaseConsonant(const uint32_t cp) { return cp >= 0x0E01 && cp <= 0x0E2E; }

// Upper vowels + NIKHAHIT + MAITAIKHU (everything on the first above-base tier).
constexpr bool isUpperVowel(const uint32_t cp) {
  return cp == 0x0E31 || (cp >= 0x0E34 && cp <= 0x0E37) || cp == 0x0E47 || cp == NIKHAHIT;
}

constexpr bool isBelowVowel(const uint32_t cp) { return cp >= 0x0E38 && cp <= 0x0E3A; }

// Tone marks + THANTHAKHAT: MAI EK..THANTHAKHAT (U+0E48-0E4C).
constexpr bool isToneMark(const uint32_t cp) { return cp >= 0x0E48 && cp <= 0x0E4C; }

// C90 PUA variant selectors.
constexpr uint32_t upperLeftVariant(const uint32_t cp) {
  if (cp >= 0x0E34 && cp <= 0x0E37) return 0xF701 + (cp - 0x0E34);
  if (cp == 0x0E31) return 0xF710;
  if (cp == NIKHAHIT) return 0xF711;
  if (cp == 0x0E47) return 0xF712;
  return cp;
}

// In Sarabun's OpenType shaping the .small tone forms sit on the second tier
// (above SARA I/II/UE/UEE), the normal forms directly above the consonant.
constexpr uint32_t toneHighVariant(const uint32_t cp) { return 0xF70A + (cp - 0x0E48); }
constexpr uint32_t toneHighLeftVariant(const uint32_t cp) { return 0xF705 + (cp - 0x0E48); }
constexpr uint32_t toneLeftVariant(const uint32_t cp) { return 0xF713 + (cp - 0x0E48); }

constexpr uint32_t belowLowVariant(const uint32_t cp) { return 0xF718 + (cp - 0x0E38); }

constexpr uint32_t descenderlessBase(const uint32_t cp) {
  if (cp == 0x0E0D) return 0xF70F;  // ญ
  if (cp == 0x0E10) return 0xF700;  // ฐ
  return cp;
}

template <typename F>
void forEachCodepoint(const std::string_view text, F&& f) {
  const auto* p = reinterpret_cast<const unsigned char*>(text.data());
  const auto* end = p + text.size();
  while (p < end && *p) {
    const uint32_t cp = utf8NextCodepoint(&p);
    if (cp == 0) break;
    if (!f(cp)) break;
  }
}

}  // namespace

bool needsShaping(const std::string_view text) {
  bool needed = false;
  uint32_t base = 0;
  bool upperVowelSeen = false;
  forEachCodepoint(text, [&](const uint32_t cp) {
    if (isBaseConsonant(cp)) {
      base = cp;
      upperVowelSeen = false;
    } else if (cp == SARA_AM) {
      needed = true;
    } else if (isUpperVowel(cp)) {
      if (isAscenderBase(base)) needed = true;
      upperVowelSeen = true;
    } else if (isBelowVowel(cp)) {
      if (isDescenderlessCandidate(base) || isDescenderBase(base)) needed = true;
    } else if (isToneMark(cp)) {
      if (upperVowelSeen || isAscenderBase(base)) needed = true;
    } else if (cp == SARA_AA || cp == 0x0E30 || cp < 0x0E00 || cp > 0x0E7F) {
      base = 0;
      upperVowelSeen = false;
    }
    return !needed;
  });
  return needed;
}

std::string shapeUtf8(const std::string_view in) {
  std::string out;
  out.reserve(in.size() + 8);

  // Cluster state, reset at each base consonant.
  uint32_t base = 0;
  bool upperVowelSeen = false;
  // Byte position in `out` where the pending tone mark of the current cluster
  // starts, for SARA AM reordering. npos when none.
  size_t toneStartInOut = std::string::npos;
  uint32_t toneOriginalCp = 0;  // unshaped tone mark of the current cluster
  // Position of the cluster's base consonant in `out`, so a later below vowel
  // can rewrite ญ/ฐ in place (all substitutes are 3 UTF-8 bytes, like the
  // originals).
  size_t baseStartInOut = std::string::npos;

  forEachCodepoint(in, [&](uint32_t cp) {
    if (isBaseConsonant(cp)) {
      base = cp;
      upperVowelSeen = false;
      toneStartInOut = std::string::npos;
      baseStartInOut = out.size();
      utf8AppendCodepoint(cp, out);
      return true;
    }

    if (cp == SARA_AM) {
      // Decompose; NIKHAHIT moves before any pending tone mark of this cluster.
      const uint32_t nikhahit = isAscenderBase(base) ? upperLeftVariant(NIKHAHIT) : NIKHAHIT;
      if (toneStartInOut != std::string::npos) {
        // The tone was shaped for the "no upper vowel" case but now rides above
        // the NIKHAHIT: rewrite it to the high form first (same byte length).
        const uint32_t highTone =
            isAscenderBase(base) ? toneHighLeftVariant(toneOriginalCp) : toneHighVariant(toneOriginalCp);
        std::string repl;
        utf8AppendCodepoint(highTone, repl);
        out.replace(toneStartInOut, repl.size(), repl);
        std::string nik;
        utf8AppendCodepoint(nikhahit, nik);
        out.insert(toneStartInOut, nik);
      } else {
        utf8AppendCodepoint(nikhahit, out);
      }
      utf8AppendCodepoint(SARA_AA, out);
      upperVowelSeen = false;
      toneStartInOut = std::string::npos;
      base = 0;
      return true;
    }

    if (isUpperVowel(cp)) {
      upperVowelSeen = true;
      if (isAscenderBase(base)) cp = upperLeftVariant(cp);
      utf8AppendCodepoint(cp, out);
      return true;
    }

    if (isBelowVowel(cp)) {
      if (isDescenderlessCandidate(base)) {
        std::string repl;
        utf8AppendCodepoint(descenderlessBase(base), repl);
        if (baseStartInOut != std::string::npos && baseStartInOut + repl.size() <= out.size()) {
          out.replace(baseStartInOut, repl.size(), repl);
        }
      } else if (isDescenderBase(base)) {
        cp = belowLowVariant(cp);
      }
      utf8AppendCodepoint(cp, out);
      return true;
    }

    if (isToneMark(cp)) {
      uint32_t shaped = cp;
      if (upperVowelSeen) {
        shaped = isAscenderBase(base) ? toneHighLeftVariant(cp) : toneHighVariant(cp);
      } else {
        shaped = isAscenderBase(base) ? toneLeftVariant(cp) : cp;
      }
      toneStartInOut = out.size();
      toneOriginalCp = cp;
      utf8AppendCodepoint(shaped, out);
      return true;
    }

    // Leading vowels, SARA A/AA, digits, punctuation, non-Thai: pass through.
    // SARA A/AA and anything outside the Thai block end the mark cluster.
    if (cp == SARA_AA || cp == 0x0E30 || cp < 0x0E00 || cp > 0x0E7F) {
      base = 0;
      upperVowelSeen = false;
      toneStartInOut = std::string::npos;
    }
    utf8AppendCodepoint(cp, out);
    return true;
  });

  return out;
}

}  // namespace ThaiShaping
