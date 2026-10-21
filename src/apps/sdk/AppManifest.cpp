#include "AppManifest.h"

#include <HalStorage.h>
#include <I18n.h>
#include <Logging.h>
#include <PersistableStore.h>

#include <cstdio>
#include <cstring>

#include "AppPath.h"
#include "apps/AppSave.h"

namespace {
bool validId(const char* s) {
  const size_t n = strlen(s);
  if (n == 0 || n >= 32) return false;
  for (const char* p = s; *p; ++p) {
    if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' || *p == '_')) return false;
  }
  return true;
}
void copy(char* dst, const size_t size, const char* src) { snprintf(dst, size, "%s", src ? src : ""); }

uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (static_cast<uint32_t>(p[3]) << 24); }

// A 32x32 1-bit uncompressed BMP into BW1 (top-down, set bit = ink). The
// palette decides which index is ink, so either polarity works.
bool loadIcon(const char* path, uint8_t* out) {
  constexpr int N = AppManifest::ICON_SIZE;
  constexpr int STRIDE = (N + 31) / 32 * 4;
  uint8_t head[62];
  uint8_t rows[N * STRIDE];
  HalFile f;
  if (!Storage.exists(path) || !Storage.openFileForRead("SDK", path, f) || f.read(head, sizeof(head)) != sizeof(head)) {
    return false;
  }
  const uint32_t offset = le32(head + 10), infoSize = le32(head + 14);
  const int32_t w = static_cast<int32_t>(le32(head + 18)), h = static_cast<int32_t>(le32(head + 22));
  const uint16_t bpp = head[28] | (head[29] << 8);
  if (head[0] != 'B' || head[1] != 'M' || infoSize != 40 || w != N || (h != N && h != -N) || bpp != 1 ||
      le32(head + 30) != 0) {
    LOG_ERR("SDK", "%s: icon must be a %dx%d 1-bit BMP", path, N, N);
    return false;
  }
  // Palette entries are B, G, R, 0; the darker one is ink.
  const int luma0 = head[54] + head[55] + head[56], luma1 = head[58] + head[59] + head[60];
  const uint8_t flip = luma1 < luma0 ? 0x00 : 0xFF;  // index 1 is ink unless index 0 is darker
  if (!f.seek(offset) || f.read(rows, sizeof(rows)) != static_cast<int>(sizeof(rows))) return false;
  for (int y = 0; y < N; ++y) {
    const uint8_t* src = rows + (h > 0 ? N - 1 - y : y) * STRIDE;  // positive height: bottom-up
    for (int b = 0; b < N / 8; ++b) out[y * (N / 8) + b] = src[b] ^ flip;
  }
  return true;
}
}  // namespace

bool AppManifest::load(const char* folder) {
  snprintf(dir, sizeof(dir), "/apps/%s", folder);
  char path[64];
  snprintf(path, sizeof(path), "%s/manifest.json", dir);
  JsonDocument doc;
  if (!PersistableStoreBase::readDocFromFile(path, doc)) return false;
  const int sdk = doc["sdk"] | 0;
  if (sdk < 1 || sdk > SDK_VERSION) {
    LOG_ERR("SDK", "%s: needs SDK %d, firmware has %d", path, sdk, SDK_VERSION);
    return false;
  }
  copy(id, sizeof(id), doc["id"] | "");
  copy(entry, sizeof(entry), doc["entry"] | "app.wasm");
  copy(nameEn, sizeof(nameEn), doc["name"]["en"] | id);
  copy(nameTh, sizeof(nameTh), doc["name"]["th"] | "");
  const unsigned memory = doc["memory"] | 16U;
  const unsigned save = doc["save"] | 0U;
  if (!validId(id) || !app_path::valid(entry) || memory == 0 || memory > MAX_MEMORY_KB ||
      save > app_save::MAX_CAPACITY) {
    LOG_ERR("SDK", "%s: invalid id, entry, memory or save", path);
    return false;
  }
  memoryKb = static_cast<uint16_t>(memory);
  saveBytes = static_cast<uint16_t>(save);
  char iconPath[112];
  hasIcon = app_path::resolve(dir, doc["icon"] | "icon.bmp", iconPath, sizeof(iconPath)) && loadIcon(iconPath, icon);
  return true;
}

const char* AppManifest::name() const { return I18N.getLanguage() == Language::TH && nameTh[0] ? nameTh : nameEn; }
