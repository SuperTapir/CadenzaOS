#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/app_context.h"
#include "cadenza/core/diagnostics.h"
#include "cadenza/core/input.h"
#include "cadenza/core/mono_canvas.h"

namespace cadenza::presentation {

enum class SurfaceKind : std::uint8_t { None, SystemMenu, Dialog };

enum class SystemMenuItem : std::uint8_t {
  Resume,
  Home,
  Sound,
  Motion,
  Count,
};

enum class SurfaceRejection : std::uint8_t {
  None,
  Busy,
  InvalidAction,
  DeniedOwner,
  TransientQueueFull,
};

enum class SystemSurfaceIntent : std::uint8_t {
  None,
  Opened,
  Closed,
  Navigate,
  Home,
  Sound,
  Motion,
  Boundary,
};

struct SystemSurfaceDiagnostics {
  std::uint32_t opened = 0;
  std::uint32_t closed = 0;
  std::uint32_t rejected = 0;
  std::uint32_t transientDropped = 0;
  std::uint8_t transientHighWater = 0;
  SurfaceRejection lastRejection = SurfaceRejection::None;
};

struct SystemSurfaceFrame {
  bool consumeInput = false;
  bool captureFrozenFrame = false;
  SystemSurfaceIntent intent = SystemSurfaceIntent::None;
};

struct TransientFeedback {
  std::array<char, 32> text{};
  float remainingSeconds = 0.0F;
};

class SystemSurfaceCoordinator {
 public:
  static constexpr std::size_t kTransientCapacity = 4;

  SystemSurfaceFrame update(Seconds dt, const InputFrame& input,
                            bool appTransitioning,
                            bool homeCurrent = false,
                            MotionProfile motionProfile =
                                MotionProfile::Normal) noexcept;
  void notifyTransitionStable() noexcept;

  bool requestInteractive(SurfaceKind kind) noexcept;
  bool rejectInvalidAction() noexcept;
  bool pushTransient(const char* text, Seconds duration) noexcept;
  void setDiagnosticSink(DiagnosticSink* sink) noexcept {
    diagnosticSink_ = sink;
  }

  bool menuActive() const noexcept {
    return interactive_ == SurfaceKind::SystemMenu;
  }
  bool menuClosing() const noexcept { return menuClosing_; }
  bool appSuspended() const noexcept {
    return menuActive() || deferredMenu_ || captureUntilRelease_;
  }
  bool hasDeferredMenu() const noexcept { return deferredMenu_; }
  SurfaceKind interactiveKind() const noexcept { return interactive_; }
  SystemMenuItem selection() const noexcept { return selection_; }
  float revealProgress() const noexcept { return revealProgress_; }
  const SystemSurfaceDiagnostics& diagnostics() const noexcept {
    return diagnostics_;
  }
  std::size_t transientCount() const noexcept { return transientCount_; }
  const TransientFeedback* transientAt(std::size_t index) const noexcept;

 private:
  SystemSurfaceFrame openMenu(bool captureFrame,
                              MotionProfile motionProfile =
                                  MotionProfile::Normal) noexcept;
  SystemSurfaceFrame closeMenu() noexcept;
  void releaseMenu() noexcept;
  void reject(SurfaceRejection reason) noexcept;
  void expireTransients(Seconds dt) noexcept;

  SurfaceKind interactive_ = SurfaceKind::None;
  SystemMenuItem selection_ = SystemMenuItem::Resume;
  bool menuArmed_ = false;
  bool captureUntilRelease_ = false;
  bool deferredMenu_ = false;
  bool openWhenStable_ = false;
  bool menuClosing_ = false;
  float revealProgress_ = 0.0F;
  MotionProfile menuMotionProfile_ = MotionProfile::Normal;
  std::array<TransientFeedback, kTransientCapacity> transients_{};
  std::size_t transientCount_ = 0;
  SystemSurfaceDiagnostics diagnostics_{};
  DiagnosticSink* diagnosticSink_ = nullptr;
};

struct SystemMenuLayout {
  Rect panel;
  Rect header;
  Rect rows;
  std::int32_t rowHeight = 0;

  static SystemMenuLayout forCanvas(std::int32_t width,
                                    std::int32_t height) noexcept;
};

class SystemUi {
 public:
  static void panel(MonoCanvas& canvas, Rect bounds) noexcept;
  static void header(MonoCanvas& canvas, Rect bounds,
                     const char* title) noexcept;
  static void menuRow(MonoCanvas& canvas, Rect bounds, const char* label,
                      const char* valueText, bool selected,
                      bool disabled) noexcept;
  static void selection(MonoCanvas& canvas, Rect bounds,
                        bool selected) noexcept;
  static void value(MonoCanvas& canvas, Rect bounds, const char* text,
                    bool inverted) noexcept;
  static void volumeIndicator(MonoCanvas& canvas, Rect bounds,
                              audio::SoundVolume volume,
                              bool inverted) noexcept;
  static void toggle(MonoCanvas& canvas, Rect bounds, bool enabled,
                     bool inverted) noexcept;
  static void statusIndicator(MonoCanvas& canvas, Rect bounds,
                              const char* label, bool active) noexcept;
};

void renderSystemMenu(MonoCanvas& canvas, SystemMenuItem selection,
                      bool homeCurrent, const SystemSnapshot& snapshot,
                      float revealProgress = 1.0F) noexcept;
void renderSystemMenu(MonoCanvas& canvas, MonoFramebuffer& scratch,
                      SystemMenuItem selection, bool homeCurrent,
                      const SystemSnapshot& snapshot, float revealProgress,
                      bool closing) noexcept;
void renderTransientFeedback(MonoCanvas& canvas,
                             const SystemSurfaceCoordinator& surfaces) noexcept;

}  // namespace cadenza::presentation
