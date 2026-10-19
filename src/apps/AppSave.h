#pragma once

#include <cstddef>
#include <cstdint>

// PERSONAL: crash-safe state for Pocket Hub apps, one file per app under
// /.crosspoint/apps/. Format and recovery rules: AppSaveFormat.h.
//
// `capacity` is the largest payload the app will ever save; it fixes the slot
// size, so keep it constant for a given `version`. A file written with another
// version loads as "no save" and the app starts fresh.
namespace app_save {

constexpr size_t MAX_CAPACITY = 1024;

// Copies the newest valid payload into `out` (at most `capacity` bytes).
// False when there is no file, no valid slot, or the version differs.
bool load(const char* appId, uint16_t version, void* out, size_t capacity, size_t* outLen = nullptr);

// Writes `data` into the older slot. False on any I/O error; the previous
// save stays readable.
bool save(const char* appId, uint16_t version, const void* data, size_t len, size_t capacity);

}  // namespace app_save
