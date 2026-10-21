#pragma once

#include <cstddef>
#include <cstdint>

// PERSONAL: random access to the rows of a TSV file on SD without loading it.
// A row is a line with a TAB that does not start with '#' (tsv_card::isCard).
// The first open writes an index of row offsets to <indexDir>/<hash>.idx; it
// is rebuilt when the file's size or modification time changes.
class TsvFile {
 public:
  static constexpr size_t MAX_LINE = 1024;  // longer rows are cut
  static constexpr uint32_t MAX_ROWS = 20000;

  bool open(const char* dir, const char* name, const char* indexDir);
  uint32_t count() const { return rowCount; }
  // Reads row `index` into `buf` (MAX_LINE + 1 bytes), NUL-terminated, without the newline.
  bool read(uint32_t index, char* buf) const;

  static uint32_t nameHash(const char* name);

 private:
  char path[96] = {};
  char indexPath[80] = {};
  uint32_t rowCount = 0;

  bool loadIndex(uint32_t fileSize, uint32_t fileTime);
  bool buildIndex(const char* indexDir, uint32_t fileSize, uint32_t fileTime);
};
