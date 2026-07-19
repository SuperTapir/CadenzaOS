#pragma once

#include "cadenza/core/app.h"
#include "cadenza/core/app_catalog.h"
#include "cadenza/core/app_context.h"
#include "cadenza/core/core_types.h"
#include "cadenza/core/diagnostics.h"
#include "cadenza/core/input.h"
#include "cadenza/core/mono_canvas.h"
#include "cadenza/core/transition.h"
#include "cadenza/presentation/defaults.h"
#include "cadenza/presentation/system_surface.h"

namespace cadenza {

class AppRuntime : public AppNavigator, public AppCapabilityResolver {
 public:
  explicit AppRuntime(
      FramebufferProfile profile = FramebufferProfile::TEmbed) noexcept;
  AppRuntime(
      FramebufferProfile profile, const Transition& transition,
      Seconds transitionDuration =
          presentation_defaults::kAppTransitionDuration) noexcept;

  bool registerApp(AppId id, App& app,
                   bool visibleInLauncher = true,
                   AppCapabilitySet capabilities = {}) noexcept;
  bool resolveCapabilities(AppId id,
                           AppCapabilitySet& capabilities) const
      noexcept override;
  bool configureHome(AppId id) noexcept;
  bool begin(AppId initial) noexcept;
  void update(Seconds dt, const InputFrame& input) noexcept;
  void updateWithSystem(Seconds dt, const InputFrame& input,
                        const SystemSnapshot& snapshot,
                        SystemCommandSink& commands) noexcept;
  void bindSystem(const SystemSnapshot& snapshot,
                  SystemCommandSink& commands) noexcept {
    frameSnapshot_ = snapshot;
    frameCommandSink_ = &commands;
  }
  void render(MonoCanvas& canvas) noexcept;
  void renderWithSystem(MonoCanvas& canvas,
                        const SystemSnapshot& snapshot) noexcept;
  bool open(AppId id) noexcept override;

  void setDiagnosticSink(DiagnosticSink* sink) noexcept {
    diagnosticSink_ = sink;
    surfaces_.setDiagnosticSink(sink);
  }
  void emitDiagnostic(const DiagnosticEvent& event) const noexcept;

  AppId currentId() const noexcept { return currentId_; }
  std::uint8_t launcherAppCount() const noexcept;
  AppId launcherAppAt(std::uint8_t position) const noexcept;
  const char* appName(AppId id) const noexcept;
  AppCatalogView catalogView() const noexcept {
    return AppCatalogView{catalog_};
  }
  bool transitioning() const noexcept { return transitioning_; }
  float transitionProgress() const noexcept { return transition_; }
  void setTransition(const Transition& transition,
                     Seconds duration) noexcept;
  std::int16_t canvasWidth() const noexcept { return outgoingFrame_.width(); }
  std::int16_t canvasHeight() const noexcept { return outgoingFrame_.height(); }
  bool systemMenuActive() const noexcept { return surfaces_.menuActive(); }
  bool appSuspendedBySystem() const noexcept {
    return surfaces_.appSuspended();
  }
  presentation::SystemMenuItem systemMenuSelection() const noexcept {
    return surfaces_.selection();
  }
  presentation::SystemSurfaceCoordinator& systemSurfaces() noexcept {
    return surfaces_;
  }
  const presentation::SystemSurfaceCoordinator& systemSurfaces()
      const noexcept {
    return surfaces_;
  }

 private:
  class DiscardingSystemCommandSink final : public SystemCommandSink {
   public:
    void bindCapabilityResolver(const AppCapabilityResolver&) noexcept
        override {}
    bool submit(const SystemOperationEnvelope&) noexcept override {
      return false;
    }
    void rejectAppOperation(AppId, SystemCommandType) noexcept override {}
  };

  bool startTransition(AppId id, audio::SoundCue cue) noexcept;
  void selectTransitionRoute(AppId destination) noexcept;
  bool renderHandoffFrame(AppId id, float progress,
                          bool preferLaunchSequence) noexcept;
  void captureFrozenApp(const SystemSnapshot& snapshot) noexcept;
  void handleSystemSurfaceIntent(
      presentation::SystemSurfaceIntent intent,
      const SystemSnapshot& snapshot) noexcept;
  void renderAppWithContext(App& app, MonoCanvas& canvas,
                            const SystemSnapshot& snapshot) noexcept;

  AppCatalog catalog_{};
  AppId homeId_{};
  AppId currentId_{};
  AppId pendingId_{};
  float transition_ = 0.0F;
  bool begun_ = false;
  bool transitioning_ = false;
  bool swapped_ = false;
  bool incomingRendered_ = false;
  bool defaultTransitionRouting_ = true;
  bool stagedTransition_ = false;
  bool launchHandoff_ = false;
  bool launchRendererAvailable_ = false;
  // 255 = unset. Otherwise last dither coverage used for the launch plate.
  std::uint8_t lastHandoffCoverage_ = 255;
  DiagnosticSink* diagnosticSink_ = nullptr;
  MonoFramebuffer outgoingFrame_;
  MonoFramebuffer incomingFrame_;
  const Transition* transitionStrategy_ = nullptr;
  Seconds transitionDuration_ = presentation_defaults::kAppTransitionDuration;
  Seconds activeTransitionDuration_ =
      presentation_defaults::kAppTransitionDuration;
  MotionProfile transitionMotionProfile_ = MotionProfile::Normal;
  SystemSnapshot frameSnapshot_{};
  DiscardingSystemCommandSink fallbackCommandSink_{};
  SystemCommandSink* frameCommandSink_ = &fallbackCommandSink_;
  presentation::SystemSurfaceCoordinator surfaces_{};
};

}  // namespace cadenza
