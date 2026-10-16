#include "HomeSummary.h"

#include <HalStorage.h>

#include <algorithm>
#include <cstdlib>

#include "ReadingStats.h"

namespace HomeSummary {
namespace {
constexpr size_t kMaxChapterBytes = 160;
std::string pathIn(const std::string& cacheDir) { return cacheDir + "/home-summary.txt"; }
}  // namespace

void write(const std::string& cacheDir, const int percent, const std::string& chapter) {
  ReadingStats::setPercent(std::clamp(percent, 0, 100));  // PERSONAL-HOOK(stats)
  if (cacheDir.empty()) return;
  std::string body = std::to_string(std::clamp(percent, 0, 100)) + "\n";
  // Keep one line; cut on a UTF-8 boundary.
  std::string line = chapter.substr(0, chapter.find('\n'));
  if (line.size() > kMaxChapterBytes) {
    size_t cut = kMaxChapterBytes;
    while (cut > 0 && (static_cast<unsigned char>(line[cut]) & 0xC0) == 0x80) --cut;
    line.resize(cut);
  }
  body += line + "\n";
  HalFile f;
  if (Storage.openFileForWrite("HSUM", pathIn(cacheDir), f)) {
    f.write(reinterpret_cast<const uint8_t*>(body.data()), body.size());
  }
}

bool read(const std::string& cacheDir, int& percent, std::string& chapter) {
  if (cacheDir.empty()) return false;
  const std::string path = pathIn(cacheDir);
  if (!Storage.exists(path.c_str())) return false;
  char buf[kMaxChapterBytes + 16] = {};
  const size_t n = Storage.readFileToBuffer(path.c_str(), buf, sizeof(buf));
  if (n == 0) return false;
  const std::string text(buf, n);
  const size_t nl = text.find('\n');
  percent = std::clamp(std::atoi(text.substr(0, nl).c_str()), 0, 100);
  chapter = nl == std::string::npos ? "" : text.substr(nl + 1);
  while (!chapter.empty() && (chapter.back() == '\n' || chapter.back() == '\r')) chapter.pop_back();
  return true;
}

std::string cacheDirFromThumbPath(const std::string& thumbPath) {
  const size_t slash = thumbPath.rfind('/');
  return slash == std::string::npos ? "" : thumbPath.substr(0, slash);
}

}  // namespace HomeSummary
