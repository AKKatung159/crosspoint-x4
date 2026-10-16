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

TEST(ThaiShaping, LowersOnlyHighToneVariantsByFontScale) {
  EXPECT_EQ(ThaiShaping::highToneLoweringPx(0xF705, 34), 5);
  EXPECT_EQ(ThaiShaping::highToneLoweringPx(0xF70E, 48), 6);
  EXPECT_EQ(ThaiShaping::highToneLoweringPx(0xF713, 34), 0);
  EXPECT_EQ(ThaiShaping::highToneLoweringPx(0x0E48, 34), 0);
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
