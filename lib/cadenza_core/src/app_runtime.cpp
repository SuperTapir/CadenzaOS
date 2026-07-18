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
  if (!transitioning_ && input.longPressed && currentId_ != homeId_) {
    startTransition(homeId_, audio::SoundCue::Back);
    return;
  }
  if (!transitioning_) {
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
  } else {
    renderAppWithContext(*current->app, canvas, frameSnapshot_);
  }
}

}  // namespace cadenza
