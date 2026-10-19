#include <gtest/gtest.h>

#include "AppSaveFormat.h"

using namespace app_save;

namespace {
uint32_t crcOf(const char* s) { return crc32End(crc32Update(crc32Begin(), s, strlen(s))); }
}  // namespace

TEST(AppSaveFormat, Crc32MatchesIeeeCheckValue) { EXPECT_EQ(crcOf("123456789"), 0xCBF43926U); }

TEST(AppSaveFormat, Crc32IsIncremental) {
  uint32_t crc = crc32Update(crc32Begin(), "12345", 5);
  crc = crc32Update(crc, "6789", 4);
  EXPECT_EQ(crc32End(crc), 0xCBF43926U);
}

TEST(AppSaveFormat, HeaderRoundTrips) {
  Header in;
  in.version = 3;
  in.length = 250;
  in.seq = 0xFFFFFFFEU;
  in.crc = 0xDEADBEEFU;
  uint8_t raw[HEADER_SIZE];
  encodeHeader(in, raw);
  EXPECT_EQ(memcmp(raw, "XPA1", 4), 0);
  Header out;
  ASSERT_TRUE(decodeHeader(raw, out));
  EXPECT_EQ(out.version, 3);
  EXPECT_EQ(out.length, 250);
  EXPECT_EQ(out.seq, 0xFFFFFFFEU);
  EXPECT_EQ(out.crc, 0xDEADBEEFU);
}

TEST(AppSaveFormat, RejectsBlankAndForeignSlots) {
  uint8_t raw[HEADER_SIZE] = {};
  Header h;
  EXPECT_FALSE(decodeHeader(raw, h));
  memset(raw, 0xFF, sizeof(raw));
  EXPECT_FALSE(decodeHeader(raw, h));
}

TEST(AppSaveFormat, NewestSlotPicksHigherSeq) {
  Header h[2];
  h[0].seq = 7;
  h[1].seq = 8;
  const bool both[2] = {true, true};
  EXPECT_EQ(newestSlot(h, both), 1);
  h[0].seq = 9;
  EXPECT_EQ(newestSlot(h, both), 0);
}

TEST(AppSaveFormat, NewestSlotSurvivesSeqWrap) {
  Header h[2];
  h[0].seq = 0xFFFFFFFFU;
  h[1].seq = 0;  // written after slot 0 wrapped
  const bool both[2] = {true, true};
  EXPECT_EQ(newestSlot(h, both), 1);
}

TEST(AppSaveFormat, TornSlotFallsBackToTheOther) {
  Header h[2];
  h[0].seq = 4;
  h[1].seq = 5;  // newer, but its write was interrupted (CRC failed)
  const bool onlyOld[2] = {true, false};
  EXPECT_EQ(newestSlot(h, onlyOld), 0);
  const bool none[2] = {false, false};
  EXPECT_EQ(newestSlot(h, none), -1);
}

TEST(AppSaveFormat, SlotStrideIncludesHeader) { EXPECT_EQ(slotStride(4), HEADER_SIZE + 4); }
