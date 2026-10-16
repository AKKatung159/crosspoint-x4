#include "ThaiUserDictionary.h"

#include <HalStorage.h>
#include <Logging.h>
#include <ThaiBrk.h>

#include <cstdint>

namespace ThaiUserDictionary {
namespace {

constexpr size_t kMaxFileBytes = 32 * 1024;

void appendFile(const std::string& path, std::string& out) {
  if (!Storage.exists(path.c_str())) return;
  HalFile f;
  if (!Storage.openFileForRead("THW", path, f)) return;
  const size_t size = f.fileSize();
  if (size == 0 || size > kMaxFileBytes) {
    if (size > kMaxFileBytes)
      LOG_ERR("THW", "Skipping %s: larger than %u bytes", path.c_str(), (unsigned)kMaxFileBytes);
    return;
  }
  const size_t start = out.size();
  out.resize(start + size);
  const int got = f.read(&out[start], size);
  out.resize(start + (got > 0 ? static_cast<size_t>(got) : 0));
  out.push_back('\n');
  LOG_INF("THW", "Loaded word list %s (%u bytes)", path.c_str(), (unsigned)size);
}

std::string stripExtension(const std::string& path) {
  const size_t slash = path.rfind('/');
  const size_t dot = path.rfind('.');
  return (dot != std::string::npos && (slash == std::string::npos || dot > slash)) ? path.substr(0, dot) : path;
}

}  // namespace

size_t loadForBook(const std::string& bookPath) {
  std::string text;
  appendFile("/thai-words.txt", text);
  const size_t slash = bookPath.rfind('/');
  if (slash != std::string::npos && slash > 0) appendFile(bookPath.substr(0, slash) + "/thai-words.txt", text);
  appendFile(stripExtension(bookPath) + ".words.txt", text);
  if (text.empty()) {
    ThaiBrk::clearUserWords();
    return 0;
  }
  const size_t count = ThaiBrk::setUserWords(text);
  LOG_INF("THW", "%u personal Thai words active (hash %08lx)", (unsigned)count,
          static_cast<unsigned long>(ThaiBrk::userWordsHash()));
  return count;
}

void unload() { ThaiBrk::clearUserWords(); }

void invalidateStaleLayout(const std::string& cacheDir, const std::string& layoutPath) {
  const std::string stampPath = cacheDir + "/thai-words.hash";
  const uint32_t current = ThaiBrk::userWordsHash();
  uint32_t recorded = 0;
  bool haveStamp = false;
  {
    HalFile f;
    if (Storage.exists(stampPath.c_str()) && Storage.openFileForRead("THW", stampPath, f)) {
      haveStamp = f.read(&recorded, sizeof(recorded)) == static_cast<int>(sizeof(recorded));
    }
  }
  // No stamp and no word list: a cache from before this feature, nothing to redo.
  if (haveStamp ? recorded == current : current == 0) return;

  if (Storage.exists(layoutPath.c_str())) {
    LOG_INF("THW", "Word list changed (%08lx -> %08lx): dropping %s", static_cast<unsigned long>(recorded),
            static_cast<unsigned long>(current), layoutPath.c_str());
    if (!Storage.removeDir(layoutPath.c_str())) Storage.remove(layoutPath.c_str());
  }
  HalFile f;
  if (Storage.openFileForWrite("THW", stampPath, f))
    f.write(reinterpret_cast<const uint8_t*>(&current), sizeof(current));
}

}  // namespace ThaiUserDictionary
