#include "AppManifest.h"

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
  return true;
}

const char* AppManifest::name() const {
  return I18N.getLanguage() == Language::TH && nameTh[0] ? nameTh : nameEn;
}
