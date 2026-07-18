#include "cadenza/core/app_runtime.h"

#include <algorithm>

namespace cadenza {

AppRuntime::AppRuntime(FramebufferProfile profile, const Transition& transition,
                       Seconds transitionDuration) noexcept
    : outgoingFrame_(profile),
      incomingFrame_(profile),
      transitionStrategy_(&transition),
      transitionDuration_(transitionDuration > 0.0F ? transitionDuration
          : presentation_defaults::kAppTransitionDuration) {}

void AppRuntime::setTransition(const Transition& transition,
                               Seconds duration) noexcept {
  if (transitioning_) return;
  transitionStrategy_ = &transition;
  if (duration > 0.0F) transitionDuration_ = duration;
}

bool AppRuntime::registerApp(AppId id, App& app,
                             bool visibleInLauncher,
                             AppCapabilitySet capabilities) noexcept {
  if (begun_) return false;
  return catalog_.registerApp(id, app, visibleInLauncher, app.name(),
                              capabilities) ==
         AppRegistrationResult::Registered;
}

bool AppRuntime::resolveCapabilities(
    AppId id, AppCapabilitySet& capabilities) const noexcept {
  const AppCatalogEntry* entry = catalog_.find(id);
  if (!entry) return false;
  capabilities = entry->capabilities;
  return true;
}

bool AppRuntime::configureHome(AppId id) noexcept {
  if (begun_ || !catalog_.find(id)) return false;
  homeId_ = id;
  return true;
}

std::uint8_t AppRuntime::launcherAppCount() const noexcept {
  std::uint8_t count = 0;
  for (std::size_t index = 0; index < catalog_.size(); ++index) {
    if (catalog_.at(index)->visibleInLauncher) ++count;
  }
  return count;
}

AppId AppRuntime::launcherAppAt(std::uint8_t position) const noexcept {
  for (std::size_t index = 0; index < catalog_.size(); ++index) {
    const AppCatalogEntry* entry = catalog_.at(index);
    if (entry->visibleInLauncher && position-- == 0) {
      return entry->id;
    }
  }
  return homeId_;
}

const char* AppRuntime::appName(AppId id) const noexcept {
  if (!id.valid()) return "INVALID";
  const AppCatalogEntry* entry = catalog_.find(id);
  return entry ? entry->name : "MISSING";
}

bool AppRuntime::begin(AppId initial) noexcept {
  const AppCatalogEntry* initialEntry = catalog_.find(initial);
  if (begun_ || !initialEntry) return false;
  if (!homeId_.valid()) homeId_ = initial;
  if (!catalog_.find(homeId_)) return false;
  currentId_ = initial;
  pendingId_ = initial;
  begun_ = true;
  initialEntry->app->onEnter();
  return true;
}

bool AppRuntime::open(AppId id) noexcept {
  return startTransition(id, audio::SoundCue::Confirm);
}

void AppRuntime::renderAppWithContext(
    App& app, MonoCanvas& canvas, const SystemSnapshot& snapshot) noexcept {
  const AppCatalogView catalog{catalog_};
  const AppRenderContext context{catalog, snapshot};
  app.render(canvas, context);
}

bool AppRuntime::startTransition(AppId id, audio::SoundCue cue) noexcept {
  const AppCatalogEntry* destination = catalog_.find(id);
  if (!begun_ || !destination || id == currentId_ || transitioning_) {
    return false;
  }
  pendingId_ = id;
  outgoingFrame_.clear(false);
  MonoCanvas outgoingCanvas{outgoingFrame_, diagnosticSink_};
  renderAppWithContext(*catalog_.find(currentId_)->app, outgoingCanvas,
                       frameSnapshot_);
  incomingFrame_ = outgoingFrame_;
  transition_ = 0.0F;
  transitioning_ = true;
  swapped_ = false;
  incomingRendered_ = false;
  frameCommandSink_->submit(
      {ResourceOwner::system(), SystemCommand::playSound(cue), 0});
  emitDiagnostic({DiagnosticCategory::Runtime, DiagnosticCode::AppTransition,
                  "app transition", static_cast<std::int32_t>(id.value())});
  return true;
}

void AppRuntime::captureFrozenApp(const SystemSnapshot& snapshot) noexcept {
  const AppCatalogEntry* current = catalog_.find(currentId_);
  if (!current) return;
  outgoingFrame_.clear(false);
  MonoCanvas frozenCanvas{outgoingFrame_, diagnosticSink_};
  renderAppWithContext(*current->app, frozenCanvas, snapshot);
}

void AppRuntime::handleSystemSurfaceIntent(
    presentation::SystemSurfaceIntent intent,
    const SystemSnapshot& snapshot) noexcept {
  using presentation::SystemSurfaceIntent;
  const auto submit = [this](const SystemCommand& command) noexcept {
    return frameCommandSink_->submit(
        {ResourceOwner::system(), command, 0});
  };
  switch (intent) {
    case SystemSurfaceIntent::None: break;
    case SystemSurfaceIntent::Opened:
      submit(SystemCommand::playSound(audio::SoundCue::Confirm));
      break;
    case SystemSurfaceIntent::Closed:
      submit(SystemCommand::playSound(audio::SoundCue::Back));
      break;
    case SystemSurfaceIntent::Navigate:
      submit(SystemCommand::playSound(audio::SoundCue::Navigate));
      break;
    case SystemSurfaceIntent::Boundary:
      submit(SystemCommand::playSound(audio::SoundCue::Boundary));
      break;
    case SystemSurfaceIntent::Home:
      startTransition(homeId_, audio::SoundCue::Back);
      break;
    case SystemSurfaceIntent::Sound: {
      audio::SoundVolume next = audio::SoundVolume::Muted;
      switch (snapshot.soundVolume) {
        case audio::SoundVolume::Muted: next = audio::SoundVolume::Low; break;
        case audio::SoundVolume::Low: next = audio::SoundVolume::Medium; break;
        case audio::SoundVolume::Medium: next = audio::SoundVolume::High; break;
        case audio::SoundVolume::High: next = audio::SoundVolume::Muted; break;
        case audio::SoundVolume::Count: next = audio::SoundVolume::High; break;
      }
      submit(SystemCommand::setSoundVolume(next));
      submit(SystemCommand::playSound(next == audio::SoundVolume::Muted
                                          ? audio::SoundCue::ToggleOff
                                          : audio::SoundCue::ToggleOn));
      break;
    }
    case SystemSurfaceIntent::Motion: {
      const MotionProfile next = snapshot.motionProfile == MotionProfile::Normal
                                     ? MotionProfile::Reduced
                                     : MotionProfile::Normal;
      submit(SystemCommand::setMotionProfile(next));
      submit(SystemCommand::playSound(next == MotionProfile::Reduced
                                          ? audio::SoundCue::ToggleOff
                                          : audio::SoundCue::ToggleOn));
      break;
    }
  }
}

void AppRuntime::emitDiagnostic(const DiagnosticEvent& event) const noexcept {
  if (diagnosticSink_) diagnosticSink_->emit(event);
}

void AppRuntime::update(Seconds dt, const InputFrame& input) noexcept {
  updateWithSystem(dt, input, frameSnapshot_, fallbackCommandSink_);
}

void AppRuntime::updateWithSystem(Seconds dt, const InputFrame& input,
                                  const SystemSnapshot& snapshot,
                                  SystemCommandSink& commands) noexcept {
  frameSnapshot_ = snapshot;
  frameCommandSink_ = &commands;
  commands.bindCapabilityResolver(*this);
  AppCatalogEntry* current = catalog_.find(currentId_);
  if (!begun_ || !current) return;
  const bool deferredMenuBeforeUpdate = surfaces_.hasDeferredMenu();
  const presentation::SystemSurfaceFrame surfaceFrame =
      surfaces_.update(dt, input, transitioning_, currentId_ == homeId_);
  if (surfaceFrame.captureFrozenFrame) {
    if (deferredMenuBeforeUpdate) {
      captureFrozenApp(snapshot);
    } else {
      outgoingFrame_ = incomingFrame_;
    }
  }
  handleSystemSurfaceIntent(surfaceFrame.intent, snapshot);
  if (!transitioning_) {
    if (surfaceFrame.consumeInput || surfaces_.appSuspended()) return;
    const AppCatalogView catalog{catalog_};
    AppCommandPort commandPort{currentId_, current->capabilities, commands};
    const AppUpdateContext context{std::max(0.0F, dt), input, catalog, *this,
                                   frameSnapshot_, commandPort, canvasWidth(),
                                   canvasHeight()};
    current->app->update(context);
    return;
  }

  transition_ += std::max(0.0F, dt) / transitionDuration_;
  if (!swapped_ && transition_ >= 0.5F) {
    current->app->onExit();
    currentId_ = pendingId_;
    current = catalog_.find(currentId_);
    current->app->onEnter();
    swapped_ = true;
    incomingRendered_ = false;
  }
  if (transition_ >= 1.0F) {
    transition_ = 1.0F;
    transitioning_ = false;
    surfaces_.notifyTransitionStable();
  }
}

void AppRuntime::render(MonoCanvas& canvas) noexcept {
  renderWithSystem(canvas, frameSnapshot_);
}

void AppRuntime::renderWithSystem(MonoCanvas& canvas,
                                  const SystemSnapshot& snapshot) noexcept {
  frameSnapshot_ = snapshot;
  const AppCatalogEntry* current = catalog_.find(currentId_);
  if (!begun_ || !current) {
    canvas.clear(false);
    canvas.text("RUNTIME: NO APP", canvas.width() / 2, canvas.height() / 2, 2,
                true, TextAlign::MiddleCenter);
    return;
  }
  if (transitioning_) {
    if (swapped_ && !incomingRendered_) {
      incomingFrame_.clear(false);
      MonoCanvas incomingCanvas{incomingFrame_, diagnosticSink_};
      renderAppWithContext(*current->app, incomingCanvas, frameSnapshot_);
      incomingRendered_ = true;
    }
    transitionStrategy_->compose(outgoingFrame_, incomingFrame_, canvas,
                                 transition_);
  } else if (surfaces_.appSuspended()) {
    canvas.drawFramebuffer(outgoingFrame_,
                           {0, 0, outgoingFrame_.width(),
                            outgoingFrame_.height()},
                           0, 0);
  } else {
    incomingFrame_.clear(false);
    MonoCanvas appCanvas{incomingFrame_, diagnosticSink_};
    renderAppWithContext(*current->app, appCanvas, frameSnapshot_);
    canvas.drawFramebuffer(incomingFrame_,
                           {0, 0, incomingFrame_.width(),
                            incomingFrame_.height()},
                           0, 0);
  }
  presentation::renderTransientFeedback(canvas, surfaces_);
  if (surfaces_.menuActive()) {
    presentation::renderSystemMenu(canvas, surfaces_.selection(),
                                   currentId_ == homeId_, frameSnapshot_,
                                   surfaces_.revealProgress());
  }
}

}  // namespace cadenza
