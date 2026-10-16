#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

/// Thai line-break opportunities (Thai is written without spaces between words).
///
/// Segmentation is dictionary based: a dynamic-programming search over a
/// flash-resident word DAWG (ThaiDictData.h) picks, for every run of Thai text,
/// the split with the fewest out-of-vocabulary codepoints, then the fewest
/// segments, then the most even word lengths ("maximal matching", the same
/// objective as PyThaiNLP's newmm and kocha01/crosspoint-halo2-custom). It never
/// breaks inside a grapheme cluster (consonant + vowels + tone marks) and keeps
/// silent finals (consonant + THANTHAKHAT) with the preceding syllable.
/// Unknown words longer than a few clusters additionally get syllable-start
/// break opportunities so long transliterated names can still wrap.
///
/// Runs at layout time only (section build / TXT pagination); the result is
/// cached, so the per-page render loop never pays for it. Working memory is
/// bounded: Thai runs are processed in windows of at most kMaxWindow codepoints.
namespace ThaiBrk {

constexpr size_t kMaxWindow = 384;

/// Thai block letters, vowels, marks and digits (U+0E01-U+0E5B), excluding the
/// currency sign U+0E3F which behaves like punctuation.
constexpr bool isThai(const uint32_t cp) { return cp >= 0x0E01 && cp <= 0x0E5B && cp != 0x0E3F; }

constexpr bool isThaiConsonant(const uint32_t cp) { return cp >= 0x0E01 && cp <= 0x0E2E; }

/// Leading vowels เ แ โ ใ ไ bind to the consonant that follows them.
constexpr bool isThaiLeadingVowel(const uint32_t cp) { return cp >= 0x0E40 && cp <= 0x0E44; }

/// Signs that bind to what precedes them and so must never start a line:
/// SARA A/AA/AM, upper/lower vowels, tone marks, LAKKHANGYAO, MAIYAMOK and
/// PAIYANNOI.
constexpr bool isThaiNoBreakBefore(const uint32_t cp) {
  return cp == 0x0E2F                        // PAIYANNOI ฯ
         || (cp >= 0x0E30 && cp <= 0x0E3A)   // SARA A .. PHINTHU
         || cp == 0x0E45 || cp == 0x0E46     // LAKKHANGYAO, MAIYAMOK ๆ
         || (cp >= 0x0E47 && cp <= 0x0E4E);  // MAITAIKHU .. YAMAKKAN
}

/// Fast byte scan: true if the UTF-8 text contains a Thai codepoint.
bool containsThai(std::string_view text);

/// Whether a line may break between two adjacent codepoints when at least one
/// of them is Thai and the dictionary has no opinion (Thai/Latin transitions,
/// or Thai fragments glued across inline-style boundaries). Purely structural:
/// never inside a cluster, never after opening or before closing punctuation.
bool boundaryAllowsBreak(uint32_t left, uint32_t right);

/// Byte offsets (end-exclusive, strictly increasing, each < text.size() and on
/// a codepoint boundary) where a line break is allowed. Empty when the text has
/// no Thai or no opportunity.
std::vector<size_t> wordBreakByteOffsets(std::string_view text);

/// Thai word count in the built-in dictionary (for diagnostics/tests).
size_t dictionaryWordCount();

/// Extra words (character names, series terms) that count as dictionary words
/// on top of the built-in DAWG, e.g. from a "thai-words.txt" next to a book.
/// Accepts UTF-8 lines; keeps pure-Thai entries of 2..kMaxUserWordLen
/// codepoints, at most kMaxUserWords of them; '#' starts a comment. Replaces any
/// previous list. Returns the number of words kept.
constexpr size_t kMaxUserWords = 1500;
constexpr size_t kMaxUserWordLen = 40;
size_t setUserWords(std::string_view wordListText);
void clearUserWords();
size_t userWordCount();
/// Stable hash of the active user words (0 when none), for cache invalidation.
uint32_t userWordsHash();

}  // namespace ThaiBrk
