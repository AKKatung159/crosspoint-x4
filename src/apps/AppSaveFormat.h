#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

// PERSONAL: on-SD layout of a Pocket Hub app save (see AppSave.h). Kept free of
// HAL types so the host tests can exercise it.
//
// A save file holds two fixed-size slots, each a 16-byte header followed by the
// payload padded to the app's slot capacity:
//   magic u32 "XPA1" | version u16 | length u16 | seq u32 | crc32 u32   (little-endian)
// Saves alternate slots, so an interrupted write can only damage the slot being
// written; the reader takes the valid slot with the newest seq.
namespace app_save {

constexpr uint32_t MAGIC = 0x31415058;  // "XPA1"
constexpr size_t HEADER_SIZE = 16;

struct Header {
  uint16_t version = 0;
  uint16_t length = 0;
  uint32_t seq = 0;
  uint32_t crc = 0;
};

// IEEE CRC-32, incremental: start from crc32Begin(), feed chunks, finish with crc32End().
constexpr uint32_t crc32Begin() { return 0xFFFFFFFFU; }
inline uint32_t crc32Update(uint32_t crc, const void* data, const size_t len) {
  const auto* p = static_cast<const uint8_t*>(data);
  for (size_t i = 0; i < len; ++i) {
    crc ^= p[i];
    for (unsigned bit = 0; bit < 8; ++bit) crc = (crc >> 1U) ^ (0xEDB88320U & (0U - (crc & 1U)));
  }
  return crc;
}
constexpr uint32_t crc32End(const uint32_t crc) { return ~crc; }

inline void encodeHeader(const Header& h, uint8_t out[HEADER_SIZE]) {
  const uint32_t magic = MAGIC;
  memcpy(out, &magic, 4);
  memcpy(out + 4, &h.version, 2);
  memcpy(out + 6, &h.length, 2);
  memcpy(out + 8, &h.seq, 4);
  memcpy(out + 12, &h.crc, 4);
}

// False when the magic does not match (empty, foreign or torn slot).
inline bool decodeHeader(const uint8_t in[HEADER_SIZE], Header& h) {
  uint32_t magic;
  memcpy(&magic, in, 4);
  if (magic != MAGIC) return false;
  memcpy(&h.version, in + 4, 2);
  memcpy(&h.length, in + 6, 2);
  memcpy(&h.seq, in + 8, 4);
  memcpy(&h.crc, in + 12, 4);
  return true;
}

// Slot index (0/1) of the newest valid record, or -1 when neither is valid.
// seq compares modulo 2^32 so it survives wrap-around.
inline int newestSlot(const Header hdr[2], const bool valid[2]) {
  if (valid[0] && valid[1]) return static_cast<int32_t>(hdr[1].seq - hdr[0].seq) > 0 ? 1 : 0;
  if (valid[0]) return 0;
  if (valid[1]) return 1;
  return -1;
}

constexpr size_t slotStride(const size_t capacity) { return HEADER_SIZE + capacity; }

}  // namespace app_save
