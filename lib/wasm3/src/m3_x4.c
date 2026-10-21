// crosspoint-x4 addition: setters that must be compiled with this library's
// flags. m3_env.h's struct layout depends on them (gas metering adds fields),
// so callers must not write IM3Runtime fields themselves.
#include "m3_env.h"
#include "m3_x4.h"

void m3_x4_SetMemoryCap(IM3Runtime runtime, uint32_t bytes) { runtime->memoryLimit = bytes; }
