#pragma once

#include <cstddef>
#include <cstdint>

// PERSONAL: /apps/<id>/manifest.json of a Pocket SDK app, e.g.
//   {"sdk": 1, "id": "2048", "version": "1.0.0", "entry": "app.wasm",
//    "name": {"en": "2048", "th": "2048"}, "memory": 16, "save": 64}
// memory: KB of wasm linear memory (default 16, at most 32); save: bytes the
// app may persist with x4_save (default 0, at most 1024).
struct AppManifest {
  static constexpr int SDK_VERSION = 1;
  static constexpr size_t MAX_MEMORY_KB = 32;

  char dir[48] = {};  // "/apps/<folder>"
  char id[32] = {};
  char nameEn[48] = {};
  char nameTh[96] = {};
  char entry[32] = {};
  uint16_t memoryKb = 16;
  uint16_t saveBytes = 0;

  // Reads and validates <dir>/manifest.json; logs and returns false when it is
  // missing, malformed, or asks for a newer SDK.
  bool load(const char* folder);
  // The name in the UI language, falling back to English.
  const char* name() const;
};
