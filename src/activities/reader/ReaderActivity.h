#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <utility>

#include "EndOfBookOptions.h"
#include "activities/Activity.h"

class ReaderActivity : public Activity {
 protected:
  std::string bookPath;
  int pagesUntilFullRefresh = 0;
  bool forcedRefreshPending = false;

  std::unique_ptr<EndOfBookOptions> endOfBookOptions;
  std::atomic<bool> endOfBookOptionsReady{false};
  std::atomic<bool> pageRendered{false};
  bool bookRemembered = false;
  void markPageRendered() { pageRendered.store(true, std::memory_order_release); }
  void rememberBookOnceRendered();

  // PERSONAL-HOOK(progress): reading position is written to the SD card every
  // kProgressWriteEvery page renders and always on exit (which sleep also runs),
  // instead of on every page turn: each save is a temp write + remove + rename,
  // i.e. several FAT updates. A crash or dead battery loses at most a few pages.
  static constexpr uint8_t kProgressWriteEvery = 5;
  uint8_t progressPending = 0;
  /// Call where a page render would save progress; true = skip it for now.
  bool deferProgressWrite() { return ++progressPending < kProgressWriteEvery; }
  void progressWritten() { progressPending = 0; }
  /// Writes the current position now (override in readers; default no-op).
  virtual void flushProgress() {}
  /// PERSONAL-HOOK(home-summary): percent + chapter for the home screen, on exit.
  virtual void writeHomeSummary() {}

  explicit ReaderActivity(const char* name, GfxRenderer& renderer, MappedInputManager& mappedInput,
                          std::string bookPath, bool allowFastInitialRefresh);

  virtual bool loadBook() = 0;
  virtual std::string getBookTitle() const = 0;
  virtual std::string getBookAuthor() const { return ""; }
  virtual std::string getBookThumbBmpPath() const { return ""; }

  virtual bool handleFormatInput() { return false; }
  virtual bool pageTurn(bool isForward) = 0;
  virtual bool skipPages(int amount) { return pageTurn(amount > 0); }
  virtual bool isAtEndOfBook() const = 0;
  virtual void onReturnFromEndOfBook() {}

  virtual void renderBook() = 0;
  virtual void applyInitialOrientation();
  virtual void onEndOfBookRendered() {}

  bool handleBackNavigation();
  /** True while the end-of-book suggestion menu is on screen and owning input. */
  bool endOfBookMenuActive() const;
  bool handleEndOfBookMenu(bool suppressConfirmRelease = false);
  bool handleEndOfBookPageTurn(bool prevTriggered, bool nextTriggered);
  void clearEndOfBookOptionsIfNeeded();
  void disableFastInitialRefresh();

 public:
  ~ReaderActivity() override = default;

  static std::unique_ptr<ReaderActivity> create(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                std::string path, bool allowFastInitialRefresh);

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(RenderLock&& lock) override;

  bool isReaderActivity() const final { return true; }
  bool handleForcedRefresh() final;
};
