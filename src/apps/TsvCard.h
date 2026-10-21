#pragma once

#include <cstring>

// PERSONAL: one Flashcards deck line, "front<TAB>back". Kept free of HAL types
// so the host tests can exercise it.
namespace tsv_card {

// A line is a card when it has a TAB and does not start with '#'. Blank and
// comment lines are skipped, so a deck can carry notes.
inline bool isCard(const char* line, const size_t len) {
  size_t i = 0;
  if (len >= 3 && memcmp(line, "\xEF\xBB\xBF", 3) == 0) i = 3;  // UTF-8 BOM (spreadsheet exports)
  return i < len && line[i] != '#' && memchr(line + i, '\t', len - i) != nullptr;
}

// Splits a NUL-terminated line in place. Drops the BOM, a trailing CR and any
// third column, and turns the two characters "\n" into a line break.
inline bool parse(char* line, const char** front, const char** back) {
  if (!isCard(line, strlen(line))) return false;
  if (memcmp(line, "\xEF\xBB\xBF", 3) == 0) line += 3;
  char* tab = strchr(line, '\t');
  *tab = '\0';
  char* end = strchr(tab + 1, '\t');
  if (!end) end = tab + 1 + strlen(tab + 1);
  *end = '\0';
  if (end > tab + 1 && end[-1] == '\r') end[-1] = '\0';
  for (char* s : {line, tab + 1}) {
    char* w = s;
    for (const char* r = s; *r; ++r, ++w) {
      if (r[0] == '\\' && r[1] == 'n') {
        *w = '\n';
        ++r;
      } else {
        *w = *r;
      }
    }
    *w = '\0';
  }
  *front = line;
  *back = tab + 1;
  return true;
}

// Splits a NUL-terminated row in place at TABs into at most `max` columns
// (the last one keeps any further TABs). Returns the column count.
inline int split(char* row, char* cols[], const int max) {
  if (memcmp(row, "\xEF\xBB\xBF", 3) == 0) row += 3;
  int n = 0;
  while (n < max) {
    cols[n++] = row;
    char* tab = n < max ? strchr(row, '\t') : nullptr;
    if (!tab) break;
    *tab = '\0';
    row = tab + 1;
  }
  return n;
}

}  // namespace tsv_card
