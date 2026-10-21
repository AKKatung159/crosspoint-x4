#include "TsvFile.h"

#include <HalStorage.h>
#include <Logging.h>
#include <Memory.h>

#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t INDEX_MAGIC = 0x31494658;  // "XFI1"
constexpr size_t INDEX_HEADER = 16;           // magic, file size, file mtime, row count

void putHeader(uint8_t out[INDEX_HEADER], const uint32_t size, const uint32_t time, const uint32_t count) {
  const uint32_t fields[4] = {INDEX_MAGIC, size, time, count};
  memcpy(out, fields, sizeof(fields));
}

}  // namespace

uint32_t TsvFile::nameHash(const char* name) {
  uint32_t h = 2166136261U;  // FNV-1a
  for (; *name; ++name) h = (h ^ static_cast<uint8_t>(*name)) * 16777619U;
  return h;
}

bool TsvFile::open(const char* dir, const char* name, const char* indexDir) {
  rowCount = 0;
  const int n = snprintf(path, sizeof(path), "%s/%s", dir, name);
  if (n <= 0 || static_cast<size_t>(n) >= sizeof(path)) return false;
  snprintf(indexPath, sizeof(indexPath), "%s/%08lx.idx", indexDir, static_cast<unsigned long>(nameHash(path)));
  uint32_t size, time;
  {
    HalFile file;
    if (!Storage.openFileForRead("TSV", path, file)) return false;
    size = static_cast<uint32_t>(file.fileSize());
    time = file.modificationTime();
  }
  return loadIndex(size, time) || buildIndex(indexDir, size, time);
}

bool TsvFile::loadIndex(const uint32_t fileSize, const uint32_t fileTime) {
  if (!Storage.exists(indexPath)) return false;
  HalFile idx;
  if (!Storage.openFileForRead("TSV", indexPath, idx)) return false;
  uint8_t raw[INDEX_HEADER], expect[INDEX_HEADER];
  if (idx.read(raw, INDEX_HEADER) != static_cast<int>(INDEX_HEADER)) return false;
  uint32_t count;
  memcpy(&count, raw + 12, 4);
  putHeader(expect, fileSize, fileTime, count);
  if (memcmp(raw, expect, INDEX_HEADER) != 0 || count > MAX_ROWS || idx.fileSize() != INDEX_HEADER + 4 * count) {
    return false;
  }
  rowCount = count;
  return true;
}

// One pass over the file in 512-byte chunks; row start offsets go to the index
// file 32 at a time, so RAM use does not grow with the file.
bool TsvFile::buildIndex(const char* indexDir, const uint32_t fileSize, const uint32_t fileTime) {
  if (!Storage.ensureDirectoryExists(indexDir)) return false;
  HalFile file, idx;
  if (!Storage.openFileForRead("TSV", path, file) || !Storage.openFileForWrite("TSV", indexPath, idx)) {
    return false;
  }
  uint8_t header[INDEX_HEADER];
  putHeader(header, fileSize, fileTime, 0);  // count patched in at the end; a crash leaves a mismatch
  if (idx.write(header, INDEX_HEADER) != INDEX_HEADER) return false;

  constexpr size_t CHUNK = 512;
  auto chunk = makeUniqueNoThrow<uint8_t[]>(CHUNK);  // heap: keeps the loop task's stack small
  if (!chunk) {
    LOG_ERR("TSV", "OOM: %u bytes", static_cast<unsigned>(CHUNK));
    return false;
  }
  uint32_t pending[32];
  size_t pendingCount = 0;
  uint32_t count = 0, offset = 0, lineStart = 0;
  char head[4];  // enough to see a BOM and the first character after it
  size_t lineLen = 0;
  bool sawTab = false;
  for (;;) {
    const int got = file.read(chunk.get(), CHUNK);
    if (got < 0) return false;
    const bool eof = got == 0;  // a last line without '\n' still ends here
    for (int i = 0; i < (eof ? 1 : got); ++i) {
      const char c = eof ? '\n' : static_cast<char>(chunk[i]);
      if (c != '\n') {
        if (lineLen < sizeof(head)) head[lineLen] = c;
        if (c == '\t') sawTab = true;
        ++lineLen;
        continue;
      }
      // Same rule as tsv_card::isCard: a TAB, and the first character after an optional BOM is not '#'.
      const size_t skip = lineLen >= 3 && memcmp(head, "\xEF\xBB\xBF", 3) == 0 ? 3 : 0;
      if (sawTab && lineLen > skip && head[skip] != '#' && count < MAX_ROWS) {
        pending[pendingCount++] = lineStart;
        ++count;
        if (pendingCount == 32) {
          if (idx.write(pending, sizeof(pending)) != sizeof(pending)) return false;
          pendingCount = 0;
        }
      }
      lineStart = offset + i + 1;
      lineLen = 0;
      sawTab = false;
    }
    if (eof) break;
    offset += got;
  }
  if (pendingCount && idx.write(pending, pendingCount * 4) != pendingCount * 4) return false;
  putHeader(header, fileSize, fileTime, count);
  if (!idx.seek(0) || idx.write(header, INDEX_HEADER) != INDEX_HEADER) return false;
  idx.flush();
  LOG_INF("TSV", "Indexed %lu rows in %s", static_cast<unsigned long>(count), path);
  rowCount = count;
  return true;
}

bool TsvFile::read(const uint32_t index, char* buf) const {
  if (index >= rowCount) return false;
  uint32_t offset;
  {
    HalFile idx;
    if (!Storage.openFileForRead("TSV", indexPath, idx) || !idx.seek(INDEX_HEADER + 4 * index) ||
        idx.read(&offset, 4) != 4) {
      return false;
    }
  }
  HalFile file;
  if (!Storage.openFileForRead("TSV", path, file) || !file.seek(offset)) return false;
  const int got = file.read(buf, MAX_LINE);
  if (got <= 0) return false;
  buf[got] = '\0';
  if (char* nl = strchr(buf, '\n')) *nl = '\0';
  const size_t len = strlen(buf);
  if (len > 0 && buf[len - 1] == '\r') buf[len - 1] = '\0';
  return true;
}
