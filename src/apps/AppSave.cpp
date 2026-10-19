#include "AppSave.h"

#include <HalStorage.h>
#include <Logging.h>

#include <cstdio>

#include "AppSaveFormat.h"

namespace app_save {
namespace {

constexpr char APPS_DIR[] = "/.crosspoint/apps";

bool pathFor(const char* appId, char* out, const size_t outSize) {
  const int n = snprintf(out, outSize, "%s/%s.sav", APPS_DIR, appId);
  return n > 0 && static_cast<size_t>(n) < outSize;
}

// Reads slot `index`'s header and checks its payload CRC by streaming it in
// small chunks, so validation needs no payload-sized buffer.
bool readValidSlot(HalFile& f, const int index, const size_t capacity, Header& h) {
  uint8_t raw[HEADER_SIZE];
  if (!f.seek(index * slotStride(capacity)) || f.read(raw, HEADER_SIZE) != static_cast<int>(HEADER_SIZE)) return false;
  if (!decodeHeader(raw, h) || h.length > capacity) return false;
  uint8_t chunk[64];
  uint32_t crc = crc32Begin();
  for (size_t left = h.length; left > 0;) {
    const size_t n = left < sizeof(chunk) ? left : sizeof(chunk);
    if (f.read(chunk, n) != static_cast<int>(n)) return false;
    crc = crc32Update(crc, chunk, n);
    left -= n;
  }
  return crc32End(crc) == h.crc;
}

bool writeZeros(HalFile& f, size_t count) {
  const uint8_t zero[32] = {};
  while (count > 0) {
    const size_t n = count < sizeof(zero) ? count : sizeof(zero);
    if (f.write(zero, n) != n) return false;
    count -= n;
  }
  return true;
}

// Newest valid slot of an open save file, or -1.
int scan(HalFile& f, const size_t capacity, Header hdr[2]) {
  bool valid[2];
  for (int i = 0; i < 2; ++i) valid[i] = readValidSlot(f, i, capacity, hdr[i]);
  return newestSlot(hdr, valid);
}

}  // namespace

bool load(const char* appId, const uint16_t version, void* out, const size_t capacity, size_t* outLen) {
  char path[64];
  if (capacity > MAX_CAPACITY || !pathFor(appId, path, sizeof(path)) || !Storage.exists(path)) return false;
  HalFile f;
  if (!Storage.openFileForRead("APPSAVE", path, f)) return false;
  Header hdr[2];
  const int slot = scan(f, capacity, hdr);
  if (slot < 0) {
    LOG_ERR("APPSAVE", "No valid slot in %s", path);
    return false;
  }
  if (hdr[slot].version != version) {
    LOG_INF("APPSAVE", "%s is version %u, want %u; starting fresh", path, hdr[slot].version, version);
    return false;
  }
  const size_t len = hdr[slot].length;
  if (!f.seek(slot * slotStride(capacity) + HEADER_SIZE) || f.read(out, len) != static_cast<int>(len)) return false;
  // Re-check the copy: the card could have changed between scan and read.
  if (crc32End(crc32Update(crc32Begin(), out, len)) != hdr[slot].crc) return false;
  if (outLen) *outLen = len;
  return true;
}

bool save(const char* appId, const uint16_t version, const void* data, const size_t len, const size_t capacity) {
  char path[64];
  if (len > capacity || capacity > MAX_CAPACITY || !pathFor(appId, path, sizeof(path))) return false;
  if (!Storage.ensureDirectoryExists(APPS_DIR)) return false;

  // O_RDWR without O_TRUNC: the other slot must survive this write.
  HalFile f = Storage.open(path, O_RDWR | O_CREAT);
  if (!f.isOpen()) {
    LOG_ERR("APPSAVE", "Cannot open %s", path);
    return false;
  }
  Header hdr[2];
  const int newest = scan(f, capacity, hdr);
  const int target = newest < 0 ? 0 : 1 - newest;
  Header h;
  h.version = version;
  h.length = static_cast<uint16_t>(len);
  h.seq = newest < 0 ? 1 : hdr[newest].seq + 1;
  h.crc = crc32End(crc32Update(crc32Begin(), data, len));

  // SdFat cannot seek past EOF, so zero-fill up to this slot's payload first.
  // A zeroed header has no magic and reads as an empty slot.
  const size_t offset = target * slotStride(capacity);
  const size_t size = f.fileSize();
  if (size < offset + HEADER_SIZE && (!f.seek(size) || !writeZeros(f, offset + HEADER_SIZE - size))) return false;
  uint8_t raw[HEADER_SIZE];
  encodeHeader(h, raw);
  // Payload before header: a torn write leaves a stale header whose CRC no longer matches.
  if (!f.seek(offset + HEADER_SIZE) || f.write(data, len) != len || !writeZeros(f, capacity - len)) return false;
  if (!f.seek(offset) || f.write(raw, HEADER_SIZE) != HEADER_SIZE) {
    LOG_ERR("APPSAVE", "Short write to %s", path);
    return false;
  }
  f.flush();
  return true;
}

}  // namespace app_save
