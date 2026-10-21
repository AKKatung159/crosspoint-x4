#pragma once
#include "wasm3.h"
#ifdef __cplusplus
extern "C" {
#endif
// Caps the linear memory bytes actually allocated, whatever the module
// declares (wasm pages are 64 KB). The resource limit would reject it instead.
void m3_x4_SetMemoryCap(IM3Runtime runtime, uint32_t bytes);
#ifdef __cplusplus
}
#endif
