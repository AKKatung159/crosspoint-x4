#include <ThaiShaping.h>
#include <Utf8.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace {

std::vector<uint32_t> codepoints(const std::string& text) {
  std::vector<uint32_t> result;
  const auto* cursor = reinterpret_cast<const unsigned char*>(text.c_str());
  while (const uint32_t cp = utf8NextCodepoint(&cursor)) result.push_back(cp);
  return result;
}

}  // namespace

// Cases from JTIAPBNAI/CrosspointTH (MIT) test/thai_shaping.
TEST(ThaiShaping, KeepsToneAtNormalTierWithoutUpperVowel) {
  EXPECT_EQ(codepoints(ThaiShaping::shapeUtf8("อ่")), (std::vector<uint32_t>{0x0E2D, 0x0E48}));
  EXPECT_EQ(codepoints(ThaiShaping::shapeUtf8("ป่")), (std::vector<uint32_t>{0x0E1B, 0xF713}));
}

TEST(ThaiShaping, SelectsHighTierAboveUpperVowels) {
  EXPECT_EQ(codepoints(ThaiShaping::shapeUtf8("อึ่")), (std::vector<uint32_t>{0x0E2D, 0x0E36, 0xF70A}));
  EXPECT_EQ(codepoints(ThaiShaping::shapeUtf8("อื้อ")), (std::vector<uint32_t>{0x0E2D, 0x0E37, 0xF70B, 0x0E2D}));
  EXPECT_EQ(codepoints(ThaiShaping::shapeUtf8("ปึ้")), (std::vector<uint32_t>{0x0E1B, 0xF703, 0xF706}));
}

TEST(ThaiShaping, ReordersSaraAmStackOntoHighTier) {
  EXPECT_EQ(codepoints(ThaiShaping::shapeUtf8("อ่ำ")), (std::vector<uint32_t>{0x0E2D, 0x0E4D, 0xF70A, 0x0E32}));
  EXPECT_EQ(codepoints(ThaiShaping::shapeUtf8("น้ำ")), (std::vector<uint32_t>{0x0E19, 0x0E4D, 0xF70B, 0x0E32}));
}

TEST(ThaiShaping, DropsDescendersUnderBelowVowels) {
  EXPECT_EQ(codepoints(ThaiShaping::shapeUtf8("ญุ")), (std::vector<uint32_t>{0xF70F, 0x0E38}));
  EXPECT_EQ(codepoints(ThaiShaping::shapeUtf8("ฐุ")), (std::vector<uint32_t>{0xF700, 0x0E38}));
  EXPECT_EQ(codepoints(ThaiShaping::shapeUtf8("ฎุ")), (std::vector<uint32_t>{0x0E0E, 0xF718}));
}

// Measured on the generated Noto Serif 14 font: SARA II top 28, high MAI EK
// (U+F70A) top 36 height 6, i.e. a 2 px gap that the old ascender/8 lowering
// turned into a 2 px collision. The correction keeps exactly 1 px.
TEST(ThaiShaping, SeatsHighToneOnePixelAboveTheVowel) {
  EXPECT_EQ(ThaiShaping::highToneAdjustPx(0xF70A, 36, 6, 28), 1);   // gap 2 -> lower 1
  EXPECT_EQ(ThaiShaping::highToneAdjustPx(0xF70B, 37, 7, 30), -1);  // touching -> raise 1
  EXPECT_EQ(ThaiShaping::highToneAdjustPx(0xF705, 30, 6, 23), 0);   // already 1 px
  EXPECT_EQ(ThaiShaping::highToneAdjustPx(0xF713, 26, 6, 28), 0);   // not a high-tier tone
  EXPECT_EQ(ThaiShaping::highToneAdjustPx(0x0E48, 26, 6, 28), 0);
}

TEST(ThaiShaping, LeavesNonThaiAndPlainThaiAlone) {
  for (const std::string s : {"Hello, world", "ภาษาไทย", "กา", "เด็ก", "Ünïcödé"}) {
    EXPECT_FALSE(ThaiShaping::needsShaping(s)) << s;
    EXPECT_EQ(ThaiShaping::shapeUtf8(s), s) << s;
  }
}

// needsShaping must agree with shapeUtf8, and shaping must be idempotent: the
// renderer shapes at draw time and may be handed text that is shaped already.
TEST(ThaiShaping, NeedsShapingAgreesAndShapingIsIdempotent) {
  for (const std::string s :
       {"ที่นี่", "เปลี่ยน", "น้ำ", "ป่า", "ฝั่ง", "ฟื้น", "กตัญญู", "ฐุ", "ปั้มน้ำมัน", "สำคัญที่สุด", "ฎีกา", "อื้ออึง", "ABC ปี่ DEF"}) {
    const std::string shaped = ThaiShaping::shapeUtf8(s);
    EXPECT_EQ(ThaiShaping::needsShaping(s), shaped != s) << s;
    EXPECT_FALSE(ThaiShaping::needsShaping(shaped)) << s;
    EXPECT_EQ(ThaiShaping::shapeUtf8(shaped), shaped) << s;
  }
}
