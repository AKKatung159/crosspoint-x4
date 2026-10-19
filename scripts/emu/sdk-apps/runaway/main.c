// Test app for scripts/emu/hub_test.sh: Confirm enters an endless loop, which
// the firmware's gas budget must stop (the app then shows "App stopped").
#include "x4.h"

static volatile int sink;

X4_EXPORT(event) int event(int ev) {
  if (ev == X4_CONFIRM)
    for (;;) ++sink;
  return 0;
}

X4_EXPORT(render) void render(void) {
  x4_text(x4_width() / 2, x4_top() + 100, "Confirm = endless loop", X4_UI12, X4_CENTER);
}
