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

namespace cadenza {

class AppRuntime : public AppNavigator, public AppCapabilityResolver {
 public:
  explicit AppRuntime(
      FramebufferProfile profile = FramebufferProfile::TEmbed,
      const Transition& transition = kVenetianBlindsTransition,
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
  DiagnosticSink* diagnosticSink_ = nullptr;
  MonoFramebuffer outgoingFrame_;
  MonoFramebuffer incomingFrame_;
  const Transition* transitionStrategy_ = nullptr;
  Seconds transitionDuration_ = presentation_defaults::kAppTransitionDuration;
  SystemSnapshot frameSnapshot_{};
  DiscardingSystemCommandSink fallbackCommandSink_{};
  SystemCommandSink* frameCommandSink_ = &fallbackCommandSink_;
};

}  // namespace cadenza
