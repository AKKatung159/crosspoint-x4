#pragma once

#include <memory>

#include "activities/Activity.h"
#include "apps/sdk/AppManifest.h"

// PERSONAL: runs one Pocket SDK app (/apps/<folder>/, a wasm module) in wasm3.
// The app owns the screen between the header and the button hints; Back always
// leaves. The API it may import is documented in sdk/include/x4.h.
class WasmAppActivity final : public Activity {
 public:
  WasmAppActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const AppManifest& manifest);
  ~WasmAppActivity() override;

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&&) override;

  struct Runtime;  // wasm3 state and what the app asked of the host

 private:
  AppManifest manifest;
  std::unique_ptr<Runtime> rt;
  bool ok = false;
  int fastRefreshes = 0;

  bool start();
  bool callEvent(int event);
};
