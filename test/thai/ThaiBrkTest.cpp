#include <ThaiBrk.h>
#include <Utf8.h>
#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

std::string segmented(const std::string& text) {
  const auto breaks = ThaiBrk::wordBreakByteOffsets(text);
  std::string out;
  size_t prev = 0;
  for (const size_t b : breaks) {
    out += text.substr(prev, b - prev);
    out += '|';
    prev = b;
  }
  out += text.substr(prev);
  return out;
}

uint32_t cpAt(const std::string& s, const size_t byteOffset) {
  const auto* p = reinterpret_cast<const unsigned char*>(s.c_str() + byteOffset);
  return utf8NextCodepoint(&p);
}

uint32_t cpBefore(const std::string& s, const size_t byteOffset) {
  size_t start = byteOffset - 1;
  while (start > 0 && (static_cast<unsigned char>(s[start]) & 0xC0) == 0x80) --start;
  return cpAt(s, start);
}

// Structural invariants every break list must satisfy.
void expectValidBreaks(const std::string& text) {
  const auto breaks = ThaiBrk::wordBreakByteOffsets(text);
  size_t prev = 0;
  for (const size_t b : breaks) {
    ASSERT_GT(b, prev);
    ASSERT_LT(b, text.size());
    ASSERT_NE(static_cast<unsigned char>(text[b]) & 0xC0, 0x80) << "break inside a UTF-8 sequence";
    const uint32_t left = cpBefore(text, b);
    const uint32_t right = cpAt(text, b);
    EXPECT_FALSE(ThaiBrk::isThaiNoBreakBefore(right)) << "line would start with a dependent mark at " << b;
    EXPECT_FALSE(ThaiBrk::isThaiLeadingVowel(left)) << "leading vowel orphaned at " << b;
    prev = b;
  }
}

}  // namespace

TEST(ThaiBrk, DetectsThai) {
  EXPECT_TRUE(ThaiBrk::containsThai("abc ไทย"));
  EXPECT_FALSE(ThaiBrk::containsThai("plain ASCII, ÀÉÎ — no Thai"));
  EXPECT_FALSE(ThaiBrk::containsThai("฿"));  // the currency sign alone is punctuation
  EXPECT_TRUE(ThaiBrk::wordBreakByteOffsets("hello world").empty());
}

TEST(ThaiBrk, DictionaryIsLoaded) { EXPECT_GT(ThaiBrk::dictionaryWordCount(), 30000u); }

TEST(ThaiBrk, SegmentsSimpleSentences) {
  EXPECT_EQ(segmented("วันนี้อากาศดี"), "วันนี้|อากาศ|ดี");
  EXPECT_EQ(segmented("เขาเดินทางไปต่างประเทศเมื่อปีที่แล้ว"), "เขา|เดินทาง|ไป|ต่าง|ประเทศ|เมื่อ|ปี|ที่แล้ว");
  EXPECT_EQ(segmented("การที่เขาทำเช่นนั้นทำให้ทุกคนตกใจ"), "การ|ที่|เขา|ทำ|เช่น|นั้น|ทำให้|ทุกคน|ตกใจ");
}

// Greedy longest-match (CrosspointTH) takes "หาม" and is then stuck with
// "เห|สี"; minimizing unknowns and segments keeps "มเหสี" whole.
TEST(ThaiBrk, AvoidsGreedyLongestMatchTrap) {
  const std::string seg = segmented("ไปหามเหสี");
  EXPECT_NE(seg.find("|มเหสี"), std::string::npos) << seg;
  EXPECT_EQ(seg.find("หาม|"), std::string::npos) << seg;
}

// Long dictionary entries that are only phrases ("ธนาคารแห่งประเทศไทย") are
// pruned at generation time so they do not become unbreakable tokens.
TEST(ThaiBrk, PhrasesAreNotSingleTokens) { EXPECT_EQ(segmented("ธนาคารแห่งประเทศไทย"), "ธนาคาร|แห่ง|ประเทศ|ไทย"); }

TEST(ThaiBrk, KeepsSilentFinalsWithTheirSyllable) {
  for (const std::string word : {"จันทร์", "ศาสตร์", "พันธุ์", "ไพรซ์"}) {
    EXPECT_TRUE(ThaiBrk::wordBreakByteOffsets(word).empty()) << segmented(word);
  }
}

TEST(ThaiBrk, NeverBreaksInsideClusters) {
  expectValidBreaks("เมื่อวานนี้ฝนตกหนักมากจนน้ำท่วมถนนหน้าบ้านของฉัน");
  expectValidBreaks("กรุงเทพมหานครอมรรัตนโกสินทร์มหินทรายุธยามหาดิลกภพ");
  expectValidBreaks("ไม่ใช่เรื่องง่ายเลยที่จะเข้าใจความรู้สึกนึกคิดของผู้อื่นๆ");
}

TEST(ThaiBrk, MaiyamokAndPaiyannoiStayAttached) {
  const std::string text = "เด็กๆเล่นกันกรุงเทพฯสนุก";
  expectValidBreaks(text);
  for (const size_t b : ThaiBrk::wordBreakByteOffsets(text)) {
    EXPECT_NE(cpAt(text, b), 0x0E46u);
    EXPECT_NE(cpAt(text, b), 0x0E2Fu);
  }
}

TEST(ThaiBrk, BreaksAtScriptTransitionsButNotAroundPunctuation) {
  EXPECT_EQ(segmented("ภาษาPythonง่าย"), "ภาษา|Python|ง่าย");
  EXPECT_EQ(segmented("(ฉันชอบอ่านหนังสือ)"), "(ฉัน|ชอบ|อ่าน|หนังสือ)");
  EXPECT_EQ(segmented("“สวัสดี”"), "“สวัสดี”");
  EXPECT_EQ(segmented("ราคา100บาท"), "ราคา|100|บาท");
  EXPECT_EQ(segmented("ไทย-อังกฤษ"), "ไทย-อังกฤษ");
}

TEST(ThaiBrk, UnknownLongWordsGetSyllableBreaks) {
  // A made-up transliterated name: no dictionary entry, but it must still be
  // breakable at syllable starts, never inside a cluster.
  const std::string name = "ซาร์ทาเนียลโวลเดอร์ไมเออร์";
  expectValidBreaks(name);
  EXPECT_FALSE(ThaiBrk::wordBreakByteOffsets(name).empty());
}

TEST(ThaiBrk, StructuralBoundaryHelper) {
  EXPECT_TRUE(ThaiBrk::boundaryAllowsBreak(0x0E01, 0x0E02));   // ก|ข
  EXPECT_FALSE(ThaiBrk::boundaryAllowsBreak(0x0E01, 0x0E48));  // ก|่
  EXPECT_FALSE(ThaiBrk::boundaryAllowsBreak(0x0E40, 0x0E01));  // เ|ก
  EXPECT_FALSE(ThaiBrk::boundaryAllowsBreak('(', 0x0E01));     // (|ก
  EXPECT_FALSE(ThaiBrk::boundaryAllowsBreak(0x0E01, ','));     // ก|,
  EXPECT_TRUE(ThaiBrk::boundaryAllowsBreak('A', 0x0E01));      // A|ก
  EXPECT_FALSE(ThaiBrk::boundaryAllowsBreak('A', 'B'));        // not Thai: not our business
}

// Long unspaced Thai crosses several DP windows; the result must stay valid and
// segment the repeated sentence the same way as on its own.
TEST(ThaiBrk, LongRunsAcrossWindows) {
  const std::string unit = "ประเทศไทยมีประชากรประมาณหกสิบหกล้านคนและมีวัฒนธรรมที่หลากหลาย";
  std::string text;
  while (text.size() < 6000) text += unit;
  expectValidBreaks(text);
  const std::string seg = segmented(text);
  const std::string unitSeg = segmented(unit);
  // Drop the last word: at a repetition seam it is followed by the next copy.
  const std::string head = unitSeg.substr(0, unitSeg.rfind('|'));
  EXPECT_NE(seg.find(head), std::string::npos) << unitSeg;
}

// From JTIAPBNAI/CrosspointTH (MIT): a long mixed Thai/ASCII paragraph must not
// overflow or produce offsets inside UTF-8 sequences.
TEST(ThaiBrk, HandlesLongMixedThaiAscii) {
  std::string line;
  const std::string fragment = "ท่ามกลางความอ่อนแอของระบบการศึกษาในโรงเรียน (Bray, 1999) shadow education — ";
  while (line.size() < 5795) line += fragment;
  ASSERT_FALSE(ThaiBrk::wordBreakByteOffsets(line).empty());
  expectValidBreaks(line);
}
