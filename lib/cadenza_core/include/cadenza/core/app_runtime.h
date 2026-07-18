#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/core_types.h"
#include "cadenza/core/diagnostics.h"
#include "cadenza/core/input.h"
#include "cadenza/core/mono_canvas.h"
#include "cadenza/core/transition.h"
#include "cadenza/presentation/defaults.h"

namespace cadenza {

enum class AppId : std::uint8_t {
  Launcher,
  Clock,
  Motion,
  Settings,
  Gallery,
  Count,
};

class AppRuntime;

class App {
 public:
  virtual ~App() = default;
  virtual const char* name() const noexcept = 0;
  virtual void onEnter() noexcept {}
  virtual void onExit() noexcept {}
  virtual void update(Seconds dt, const InputFrame& input,
                      AppRuntime& runtime) noexcept = 0;
  virtual void render(MonoCanvas& canvas,
                      const AppRuntime& runtime) noexcept = 0;
};

class AppRuntime {
 public:
  explicit AppRuntime(
      FramebufferProfile profile = FramebufferProfile::TEmbed,
      const Transition& transition = kVenetianBlindsTransition,
      Seconds transitionDuration =
          presentation_defaults::kAppTransitionDuration) noexcept;

  bool registerApp(AppId id, App& app,
                   bool visibleInLauncher = true) noexcept;
  bool begin(AppId initial) noexcept;
  void update(Seconds dt, const InputFrame& input) noexcept;
  void render(MonoCanvas& canvas) noexcept;
  bool open(AppId id) noexcept;

  void setDiagnosticSink(DiagnosticSink* sink) noexcept {
    diagnosticSink_ = sink;
  }
  void emitDiagnostic(const DiagnosticEvent& event) const noexcept;

  AppId currentId() const noexcept { return currentId_; }
  std::uint8_t launcherAppCount() const noexcept;
  AppId launcherAppAt(std::uint8_t position) const noexcept;
  const char* appName(AppId id) const noexcept;
  bool transitioning() const noexcept { return transitioning_; }
  float transitionProgress() const noexcept { return transition_; }
  void setTransition(const Transition& transition,
                     Seconds duration) noexcept;
  void setMotionProfile(MotionProfile profile) noexcept {
    motionProfile_ = profile;
  }
  MotionProfile motionProfile() const noexcept { return motionProfile_; }
  std::int16_t canvasWidth() const noexcept { return outgoingFrame_.width(); }
  std::int16_t canvasHeight() const noexcept { return outgoingFrame_.height(); }

 private:
  static constexpr std::size_t kAppCapacity =
      static_cast<std::size_t>(AppId::Count);

  std::array<App*, kAppCapacity> apps_{};
  std::array<bool, kAppCapacity> visibleInLauncher_{};
  AppId currentId_ = AppId::Launcher;
  AppId pendingId_ = AppId::Launcher;
  float transition_ = 0.0F;
  bool begun_ = false;
  bool transitioning_ = false;
  bool swapped_ = false;
  DiagnosticSink* diagnosticSink_ = nullptr;
  MonoFramebuffer outgoingFrame_;
  MonoFramebuffer incomingFrame_;
  const Transition* transitionStrategy_ = nullptr;
  Seconds transitionDuration_ = presentation_defaults::kAppTransitionDuration;
  MotionProfile motionProfile_ = MotionProfile::Normal;
};

}  // namespace cadenza
