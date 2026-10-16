#include "ThaiBrk.h"

#include <Utf8.h>

#include <algorithm>

#include "ThaiDictData.h"

namespace ThaiBrk {
namespace {

constexpr uint32_t THANTHAKHAT = 0x0E4C;  // การันต์: silences the consonant(s) before it
constexpr uint32_t SARA_I = 0x0E34;
constexpr uint32_t SARA_U = 0x0E38;

// --- Dictionary DAWG (see scripts/thai/gen_thai_dict.py for the layout) -----

// 24-bit little-endian child offsets are read byte by byte: the blob is packed
// and the ESP32-C3 (RISC-V) faults on unaligned wide loads from flash.
inline uint32_t readOffset(const uint8_t* p) {
  return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) | (static_cast<uint32_t>(p[2]) << 16);
}

constexpr uint32_t NO_NODE = 0xFFFFFFFFu;

// Follows the edge labelled `ch` (codepoint - U+0E00) out of `node`. Edges are
// sorted by label, so this is a binary search over at most 127 entries.
uint32_t step(const uint32_t node, const uint8_t ch) {
  const uint8_t* n = thaiDict::DAWG + node;
  uint32_t lo = 0;
  uint32_t hi = n[0] & 0x7F;
  const uint8_t* edges = n + 1;
  while (lo < hi) {
    const uint32_t mid = (lo + hi) / 2;
    const uint8_t label = edges[mid * 4];
    if (label == ch) return readOffset(edges + mid * 4 + 1);
    if (label < ch) {
      lo = mid + 1;
    } else {
      hi = mid;
    }
  }
  return NO_NODE;
}

inline bool isTerminal(const uint32_t node) { return (thaiDict::DAWG[node] & 0x80) != 0; }

// --- User words (thai-words.txt): RAM, sorted, bucketed by first letter ------

// Each word is stored as one byte per codepoint (cp - U+0E00), all Thai.
std::vector<std::string> gUserWords;
// gUserBucket[b] .. gUserBucket[b + 1] index the words starting with byte b.
std::vector<uint16_t> gUserBucket;
uint32_t gUserHash = 0;
constexpr size_t kBuckets = 0x5C + 1;

// --- Structural rules ------------------------------------------------------

constexpr bool isThaiDigit(const uint32_t cp) { return cp >= 0x0E50 && cp <= 0x0E59; }

// Break allowed between Thai codepoints `left` and `right` by cluster rules.
constexpr bool clusterBreakAllowed(const uint32_t left, const uint32_t right) {
  if (isThaiLeadingVowel(left)) return false;
  if (isThaiNoBreakBefore(right)) return false;
  if (isThaiDigit(left) && isThaiDigit(right)) return false;
  return true;
}

bool isOpeningPunct(const uint32_t cp) {
  switch (cp) {
    case '(':
    case '[':
    case '{':
    case '"':
    case '\'':
    case '<':
    case 0x2018:  // ‘
    case 0x201C:  // “
    case 0x00AB:  // «
      return true;
    default:
      return false;
  }
}

bool isClosingPunct(const uint32_t cp) {
  switch (cp) {
    case ')':
    case ']':
    case '}':
    case '"':
    case '\'':
    case '>':
    case ',':
    case '.':
    case ';':
    case ':':
    case '!':
    case '?':
    case '%':
    case 0x2019:  // ’
    case 0x201D:  // ”
    case 0x00BB:  // »
    case 0x2026:  // …
      return true;
    default:
      return false;
  }
}

// Non-Thai codepoints next to which a Thai/other transition must not break.
bool isGluePunct(const uint32_t cp) {
  return cp == '-' || cp == '/' || cp == 0x2010 || cp == 0x2011 || cp == 0x2013 || cp == 0x2014 || cp == 0x00A0 ||
         cp == 0x200D;
}

struct Char {
  uint32_t cp;
  uint32_t end;  // byte offset just past this codepoint
};

// Per-position DP state. Scores compare lexicographically:
// fewest unknown codepoints, then fewest segments, then largest sum of squared
// known-word lengths (prefers "ตาก|ลม" style splits into long real words).
struct Cell {
  uint16_t next;
  uint16_t unknown;
  uint16_t segments;
  uint8_t known;
  uint8_t valid;
  int32_t sq;
};

bool better(const uint16_t unknown, const uint16_t segments, const int32_t sq, const Cell& cur) {
  if (!cur.valid) return true;
  if (unknown != cur.unknown) return unknown < cur.unknown;
  if (segments != cur.segments) return segments < cur.segments;
  return sq > cur.sq;
}

// Syllable-start heuristic inside an out-of-vocabulary segment [s, e): a
// leading vowel, or a consonant that carries its own vowel/mark and is not the
// second consonant of an initial cluster (กร กล กว ...).
bool looksLikeSyllableStart(const Char* c, const size_t i, const size_t s, const size_t e) {
  const uint32_t cp = c[i].cp;
  if (isThaiLeadingVowel(cp)) return true;
  if (!isThaiConsonant(cp) || i + 1 >= e) return false;
  if (isThaiLeadingVowel(c[i - 1].cp)) return false;
  const uint32_t next = c[i + 1].cp;
  const bool carriesVowel = (next >= 0x0E30 && next <= 0x0E3A) || (next >= 0x0E47 && next <= 0x0E4D);
  if (!carriesVowel || next == THANTHAKHAT) return false;
  const bool clusterSecond = (cp == 0x0E23 || cp == 0x0E25 || cp == 0x0E27) && i > s && isThaiConsonant(c[i - 1].cp);
  return !clusterSecond;
}

constexpr bool isToneMark(const uint32_t cp) { return cp >= 0x0E48 && cp <= 0x0E4B; }

// Thai Character Cluster style constraints (after Theeramunkong et al.) on top
// of clusterBreakAllowed(): true if a break before c[k] would split a syllable
// whose shape is fixed by its vowel pattern, independent of the dictionary.
// These matter most for out-of-vocabulary names, where the DP would otherwise
// happily cut "เนียล" into "เนี|ยล".
bool insideSyllable(const Char* c, const size_t k, const size_t n) {
  const uint32_t cur = c[k].cp;
  const uint32_t prev = c[k - 1].cp;
  if (!isThaiConsonant(cur)) return false;

  // Silent finals: "จันทร์", "ศาสตร์", "พันธุ์", "ไพรซ์".
  if (k + 1 < n) {
    const uint32_t n1 = c[k + 1].cp;
    if (n1 == THANTHAKHAT) return true;
    if (k + 2 < n && c[k + 2].cp == THANTHAKHAT && (isThaiConsonant(n1) || n1 == SARA_I || n1 == SARA_U)) {
      return true;
    }
  }

  // MAI HAN-AKAT is always closed by the next consonant: "กัน", "ตัว".
  if (prev == 0x0E31) return true;

  // Walk back over tone marks to the vowel that governs the syllable.
  size_t j = k;
  while (j > 0 && isToneMark(c[j - 1].cp)) --j;
  if (j == 0) return false;
  const uint32_t vowel = c[j - 1].cp;

  // -ัว: "กลัว", "หัว" (with or without a tone between).
  if (cur == 0x0E27 && vowel == 0x0E31) return true;

  // Leading-vowel patterns: find the เ/แ that opens this syllable (at most two
  // initial consonants back from the vowel, or from k for เ-อ).
  const auto opensWith = [&](size_t from, const uint32_t lead1, const uint32_t lead2) {
    size_t consonants = 0;
    while (from > 0 && isThaiConsonant(c[from - 1].cp) && consonants < 2) {
      --from;
      ++consonants;
    }
    return consonants > 0 && from > 0 && (c[from - 1].cp == lead1 || c[from - 1].cp == lead2);
  };
  // เ-ีย / เ-ือ: "เปลี่ยน", "เมือง".
  if ((cur == 0x0E22 || cur == 0x0E2D) && (vowel == 0x0E35 || vowel == 0x0E37) && j >= 2) {
    if (opensWith(j - 1, 0x0E40, 0x0E40)) return true;
  }
  // เ-อ: "เธอ", "เดอร์" (no vowel sign, only tones between).
  // (Not when the consonant before is อ itself: "เธอ|อยู่".)
  if (cur == 0x0E2D && j == k && prev != 0x0E2D && opensWith(k, 0x0E40, 0x0E40)) return true;
  // เ-็ / แ-็ are closed by a final consonant: "เด็ก", "แข็ง", "เป็น".
  if (vowel == 0x0E47 && j >= 2 && opensWith(j - 1, 0x0E40, 0x0E41)) return true;
  return false;
}

class Segmenter {
 public:
  explicit Segmenter(std::vector<size_t>& out) : out_(out) {}

  // Segments the Thai run c[0..n) and appends interior break byte offsets.
  void run(const Char* c, const size_t n) {
    size_t start = 0;
    while (start < n) {
      const size_t len = std::min(n - start, kMaxWindow);
      const bool lastWindow = start + len == n;
      start = window(c + start, len, lastWindow) + start;
    }
  }

 private:
  struct Seg {
    size_t start;
    size_t end;
    bool known;
  };

  std::vector<size_t>& out_;
  std::vector<uint8_t> boundary_;
  std::vector<Cell> cells_;
  std::vector<Seg> segs_;

  // Segments one window; returns how many codepoints were consumed (the next
  // window starts there). A non-final window gives back its last segment so a
  // word cut by the window edge is re-segmented with its full context.
  size_t window(const Char* c, const size_t n, const bool lastWindow) {
    boundary_.assign(n + 1, 0);
    boundary_[0] = 1;
    boundary_[n] = 1;
    for (size_t k = 1; k < n; ++k) {
      boundary_[k] = (clusterBreakAllowed(c[k - 1].cp, c[k].cp) && !insideSyllable(c, k, n)) ? 1 : 0;
    }

    cells_.assign(n + 1, Cell{0, 0, 0, 0, 0, 0});
    cells_[n] = Cell{static_cast<uint16_t>(n), 0, 0, 0, 1, 0};
    for (size_t i = n; i-- > 0;) {
      if (!boundary_[i]) continue;
      Cell best{0, 0, 0, 0, 0, 0};

      // Every dictionary word starting at i that ends on a boundary.
      uint32_t node = 0;
      for (size_t j = i; j < n; ++j) {
        const uint32_t cp = c[j].cp;
        node = step(node, static_cast<uint8_t>(cp - 0x0E00));
        if (node == NO_NODE) break;
        if (isTerminal(node) && boundary_[j + 1] && cells_[j + 1].valid) {
          const Cell& tail = cells_[j + 1];
          const int32_t len = static_cast<int32_t>(j + 1 - i);
          const uint16_t segs = static_cast<uint16_t>(tail.segments + 1);
          const int32_t sq = tail.sq + len * len;
          if (better(tail.unknown, segs, sq, best)) {
            best = Cell{static_cast<uint16_t>(j + 1), tail.unknown, segs, 1, 1, sq};
          }
        }
      }

      // User words starting at i (character names, series terms).
      if (!gUserWords.empty()) {
        const auto first = static_cast<uint8_t>(c[i].cp - 0x0E00);
        for (size_t w = gUserBucket[first]; w < gUserBucket[first + 1]; ++w) {
          const std::string& word = gUserWords[w];
          const size_t end = i + word.size();
          if (end > n || !boundary_[end] || !cells_[end].valid) continue;
          bool match = true;
          for (size_t k = 1; k < word.size() && match; ++k) {
            match = c[i + k].cp - 0x0E00 == static_cast<uint8_t>(word[k]);
          }
          if (!match) continue;
          const Cell& tail = cells_[end];
          const int32_t len = static_cast<int32_t>(word.size());
          const uint16_t segs = static_cast<uint16_t>(tail.segments + 1);
          const int32_t sq = tail.sq + len * len;
          if (better(tail.unknown, segs, sq, best)) {
            best = Cell{static_cast<uint16_t>(end), tail.unknown, segs, 1, 1, sq};
          }
        }
      }

      // Out-of-vocabulary fallback: one grapheme cluster.
      size_t e = i + 1;
      while (e < n && !boundary_[e]) ++e;
      if (cells_[e].valid) {
        const Cell& tail = cells_[e];
        const uint16_t unk = static_cast<uint16_t>(tail.unknown + (e - i));
        const uint16_t segs = static_cast<uint16_t>(tail.segments + 1);
        if (better(unk, segs, tail.sq, best)) {
          best = Cell{static_cast<uint16_t>(e), unk, segs, 0, 1, tail.sq};
        }
      }
      cells_[i] = best;
    }

    // Walk the chosen path, merging adjacent unknown clusters into one segment,
    // and a lone bare consonant into the segment before it: around unknown
    // names the DP likes to strand a final consonant ("โว|ล", "เพ|น"), and
    // dropping a break opportunity is always safe.
    segs_.clear();
    for (size_t i = 0; i < n; i = cells_[i].next) {
      const bool known = cells_[i].known != 0;
      const size_t e = cells_[i].next;
      const bool loneConsonant = e == i + 1 && isThaiConsonant(c[i].cp);
      if (!segs_.empty() && ((!known && !segs_.back().known) || loneConsonant)) {
        segs_.back().end = e;
        segs_.back().known = segs_.back().known && known;
      } else {
        segs_.push_back(Seg{i, e, known});
      }
    }

    size_t lastSegStart = 0;
    const size_t mark = out_.size();
    for (const Seg& s : segs_) {
      lastSegStart = s.start;
      if (!s.known && s.end - s.start >= 6) {
        for (size_t k = s.start + 1; k < s.end; ++k) {
          if (boundary_[k] && looksLikeSyllableStart(c, k, s.start, s.end)) out_.push_back(c[k - 1].end);
        }
      }
      if (s.end < n) out_.push_back(c[s.end - 1].end);
    }

    if (lastWindow) return n;
    if (lastSegStart == 0) {
      // One segment spans the whole window (a huge out-of-vocabulary run).
      // Cut at the last structural boundary; the window edge itself is not a
      // dictionary decision. Only a window made entirely of one "cluster"
      // (pathological input) is cut hard.
      size_t cut = n - 1;
      while (cut > 0 && !boundary_[cut]) --cut;
      if (cut == 0) cut = n;
      while (out_.size() > mark && out_.back() >= c[cut - 1].end) out_.pop_back();
      out_.push_back(c[cut - 1].end);
      return cut;
    }
    // Drop breaks inside/after the last segment; the next window redoes it.
    const uint32_t keepUpTo = c[lastSegStart - 1].end;
    while (out_.size() > mark && out_.back() > keepUpTo) out_.pop_back();
    return lastSegStart;
  }
};

}  // namespace

bool containsThai(const std::string_view text) {
  // Thai U+0E01..U+0E5B encodes as E0 B8 81..E0 B9 9B (U+0E3F ฿ is E0 B8 BF).
  for (size_t i = 0; i + 2 < text.size(); ++i) {
    if (static_cast<uint8_t>(text[i]) != 0xE0) continue;
    const auto b1 = static_cast<uint8_t>(text[i + 1]);
    const auto b2 = static_cast<uint8_t>(text[i + 2]);
    if (b1 == 0xB8 && b2 >= 0x81 && b2 != 0xBF) return true;
    if (b1 == 0xB9 && b2 <= 0x9B) return true;
  }
  return false;
}

bool boundaryAllowsBreak(const uint32_t left, const uint32_t right) {
  const bool lt = isThai(left);
  const bool rt = isThai(right);
  if (!lt && !rt) return false;
  if (lt && rt) return clusterBreakAllowed(left, right);
  if (lt && isThaiLeadingVowel(left)) return false;
  if (rt && isThaiNoBreakBefore(right)) return false;
  const uint32_t other = lt ? right : left;
  if (other == 0 || other == ' ' || other == '\t' || other == '\n') return false;
  if (isGluePunct(other)) return false;
  if (!lt && isOpeningPunct(left)) return false;
  if (!rt && isClosingPunct(right)) return false;
  // Thai text uses "ๆ"/"ฯ" and Latin quotes; anything else adjacent (Latin
  // words, digits, CJK) is a legitimate transition point.
  return true;
}

std::vector<size_t> wordBreakByteOffsets(const std::string_view text) {
  if (!containsThai(text)) return {};

  // Decode once into an exact-size table: a geometric regrowth of this vector
  // on a long mixed-script paragraph is the allocation most likely to fail on
  // the C3's fragmented heap.
  size_t count = 0;
  {
    const auto* p = reinterpret_cast<const unsigned char*>(text.data());
    const auto* end = p + text.size();
    while (p < end && *p) {
      utf8NextCodepoint(&p);
      ++count;
    }
  }
  if (count < 2) return {};

  std::vector<Char> chars;
  chars.reserve(count);
  {
    const auto* base = reinterpret_cast<const unsigned char*>(text.data());
    const auto* p = base;
    const auto* end = p + text.size();
    while (p < end && *p) {
      const uint32_t cp = utf8NextCodepoint(&p);
      chars.push_back(Char{cp, static_cast<uint32_t>(p - base)});
    }
  }

  std::vector<size_t> breaks;
  breaks.reserve(count / 3 + 4);
  Segmenter segmenter(breaks);

  size_t i = 0;
  while (i < chars.size()) {
    if (!isThai(chars[i].cp)) {
      ++i;
      if (i < chars.size() && isThai(chars[i].cp) && boundaryAllowsBreak(chars[i - 1].cp, chars[i].cp)) {
        breaks.push_back(chars[i - 1].end);
      }
      continue;
    }
    size_t j = i;
    while (j < chars.size() && isThai(chars[j].cp)) ++j;
    segmenter.run(chars.data() + i, j - i);
    if (j < chars.size() && boundaryAllowsBreak(chars[j - 1].cp, chars[j].cp)) {
      breaks.push_back(chars[j - 1].end);
    }
    i = j;
  }

  // Window hand-over can re-add a cut offset; keep the contract strict.
  std::sort(breaks.begin(), breaks.end());
  breaks.erase(std::unique(breaks.begin(), breaks.end()), breaks.end());
  while (!breaks.empty() && breaks.back() >= text.size()) breaks.pop_back();
  return breaks;
}

size_t dictionaryWordCount() { return thaiDict::WORD_COUNT; }

size_t setUserWords(const std::string_view text) {
  std::vector<std::string> words;
  size_t pos = 0;
  while (pos < text.size() && words.size() < kMaxUserWords) {
    size_t eol = text.find('\n', pos);
    if (eol == std::string_view::npos) eol = text.size();
    std::string_view line = text.substr(pos, eol - pos);
    pos = eol + 1;
    if (const size_t hash = line.find('#'); hash != std::string_view::npos) line = line.substr(0, hash);
    // Decode; keep the line only if every codepoint (ignoring spaces/CR/BOM) is Thai.
    std::string bytes;
    bool ok = true;
    const auto* p = reinterpret_cast<const unsigned char*>(line.data());
    const auto* end = p + line.size();
    while (p < end && ok) {
      const uint32_t cp = utf8NextCodepoint(&p);
      if (cp == ' ' || cp == 0x09 || cp == 0x0D || cp == 0xFEFF) continue;  // space, tab, CR, BOM
      if (!isThai(cp)) {
        ok = false;
        break;
      }
      bytes.push_back(static_cast<char>(cp - 0x0E00));
    }
    if (ok && bytes.size() >= 2 && bytes.size() <= kMaxUserWordLen) words.push_back(std::move(bytes));
  }
  std::sort(words.begin(), words.end());
  words.erase(std::unique(words.begin(), words.end()), words.end());

  gUserBucket.assign(kBuckets + 1, 0);
  uint32_t h = 2166136261u;  // FNV-1a over the sorted list
  for (const std::string& w : words) {
    gUserBucket[static_cast<uint8_t>(w[0]) + 1]++;
    for (const char ch : w) h = (h ^ static_cast<uint8_t>(ch)) * 16777619u;
    h = (h ^ 0xFFu) * 16777619u;
  }
  for (size_t b = 1; b <= kBuckets; ++b) gUserBucket[b] += gUserBucket[b - 1];
  gUserWords = std::move(words);
  gUserHash = gUserWords.empty() ? 0 : (h ? h : 1);
  return gUserWords.size();
}

void clearUserWords() {
  std::vector<std::string>().swap(gUserWords);
  std::vector<uint16_t>().swap(gUserBucket);
  gUserHash = 0;
}

size_t userWordCount() { return gUserWords.size(); }

uint32_t userWordsHash() { return gUserHash; }

}  // namespace ThaiBrk
