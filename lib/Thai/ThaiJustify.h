#pragma once

#include <cstddef>
#include <cstdint>

#include "ThaiBrk.h"

/// Justification policy for Thai.
///
/// Thai has no spaces between words; a space marks a phrase or sentence
/// boundary. Giving the invisible dictionary-word gaps the same stretch as real
/// spaces opens holes in the middle of phrases, so spare width goes to real
/// spaces first and each Thai word gap takes at most kMaxThaiGapPx, until the
/// spaces reach kMaxSpaceStretchPx; past that, or on a line with no real space,
/// the Thai word gaps share the rest evenly, up to kMaxThaiShareGapPx each.
/// Whatever is still left stays at the end of the line (slightly ragged): a few
/// short lines read better than holes between words.
/// Nothing is ever added inside a word (a grapheme cluster would break apart).
namespace ThaiJustify {

constexpr int kMaxThaiGapPx = 1;
/// Beyond this much extra per real space, the remainder goes to Thai gaps.
constexpr int kMaxSpaceStretchPx = 10;
/// The most a Thai word gap ever stretches (PERSONAL, 1.6.5-th.4; it was uncapped).
constexpr int kMaxThaiShareGapPx = 3;

constexpr int capThaiShare(const int px) { return px < kMaxThaiShareGapPx ? px : kMaxThaiShareGapPx; }

/// A zero-width break between two Thai words (as opposed to a space or a
/// CJK/Latin boundary).
constexpr bool isThaiWordGap(const bool noSpaceBefore, const uint32_t leftLastCp, const uint32_t rightFirstCp) {
  return noSpaceBefore && ThaiBrk::isThai(leftLastCp) && ThaiBrk::isThai(rightFirstCp);
}

struct Extra {
  int thaiGap;   // pixels added at each Thai word gap
  int otherGap;  // pixels added at every other justifiable gap
};

/// Splits `spare` pixels over `thaiGaps` Thai word gaps and `otherGaps` other
/// justifiable gaps. With no Thai gaps this is the upstream even split.
constexpr Extra distribute(const int spare, const size_t thaiGaps, const size_t otherGaps) {
  if (spare <= 0 || thaiGaps + otherGaps == 0) return {0, 0};
  if (thaiGaps == 0) return {0, spare / static_cast<int>(otherGaps)};
  if (otherGaps == 0) return {capThaiShare(spare / static_cast<int>(thaiGaps)), 0};
  const int even = spare / static_cast<int>(thaiGaps + otherGaps);
  const int thai = even < kMaxThaiGapPx ? even : kMaxThaiGapPx;
  const int other = (spare - thai * static_cast<int>(thaiGaps)) / static_cast<int>(otherGaps);
  if (other <= kMaxSpaceStretchPx) return {thai, other};
  // One or two spaces would swallow a huge gap: cap them and let the Thai word
  // gaps share the remainder evenly.
  return {capThaiShare((spare - kMaxSpaceStretchPx * static_cast<int>(otherGaps)) / static_cast<int>(thaiGaps)),
          kMaxSpaceStretchPx};
}

}  // namespace ThaiJustify
