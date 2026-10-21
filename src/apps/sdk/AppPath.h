#pragma once

#include <cstddef>
#include <cstdio>

// PERSONAL: Pocket SDK file access. Apps name files relative to their own
// folder; this is the only gate between an app and the card.
namespace app_path {

// A safe relative path: 1-63 chars of [A-Za-z0-9._-/], no leading '/', no
// empty, "." or ".." segment.
inline bool valid(const char* rel) {
  size_t len = 0, seg = 0;
  bool dotsOnly = true;
  for (const char* p = rel;; ++p, ++len) {
    const char c = *p;
    if (c == '/' || c == '\0') {
      if (seg == 0 || (dotsOnly && seg <= 2)) return false;
      if (c == '\0') return len < 64;
      seg = 0;
      dotsOnly = true;
      continue;
    }
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' ||
                    c == '_' || c == '-';
    if (!ok || len >= 63) return false;
    dotsOnly &= c == '.';
    ++seg;
  }
}

// "/apps/<id>/<rel>" into out; false when rel is not valid() or it does not fit.
inline bool resolve(const char* appDir, const char* rel, char* out, const size_t outSize) {
  if (!valid(rel)) return false;
  const int n = snprintf(out, outSize, "%s/%s", appDir, rel);
  return n > 0 && static_cast<size_t>(n) < outSize;
}

}  // namespace app_path
