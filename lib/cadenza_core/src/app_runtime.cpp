#include "cadenza/core/app_runtime.h"

#include <algorithm>

namespace cadenza {
namespace {
constexpr std::size_t indexOf(AppId id) noexcept {
  return static_cast<std::size_t>(id);
}

constexpr bool valid(AppId id) noexcept {
  return indexOf(id) < indexOf(AppId::Count);
}
}  // namespace

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
                             bool visibleInLauncher) noexcept {
  if (!valid(id) || begun_) return false;
  apps_[indexOf(id)] = &app;
  visibleInLauncher_[indexOf(id)] = visibleInLauncher;
  return true;
}

std::uint8_t AppRuntime::launcherAppCount() const noexcept {
  std::uint8_t count = 0;
  for (std::size_t index = 0; index < kAppCapacity; ++index) {
    if (apps_[index] && visibleInLauncher_[index]) ++count;
  }
  return count;
}

AppId AppRuntime::launcherAppAt(std::uint8_t position) const noexcept {
  for (std::size_t index = 0; index < kAppCapacity; ++index) {
    if (apps_[index] && visibleInLauncher_[index] && position-- == 0) {
      return static_cast<AppId>(index);
    }
  }
  return AppId::Launcher;
}

const char* AppRuntime::appName(AppId id) const noexcept {
  if (!valid(id)) return "INVALID";
  const App* app = apps_[indexOf(id)];
  return app ? app->name() : "MISSING";
}

bool AppRuntime::begin(AppId initial) noexcept {
  if (begun_ || !valid(initial) || !apps_[indexOf(initial)]) return false;
  currentId_ = initial;
  pendingId_ = initial;
  begun_ = true;
  apps_[indexOf(currentId_)]->onEnter();
  return true;
}

bool AppRuntime::open(AppId id) noexcept {
  if (!begun_ || !valid(id) || id == currentId_ || transitioning_ ||
      !apps_[indexOf(id)]) {
    return false;
  }
  pendingId_ = id;
  outgoingFrame_.clear(false);
  MonoCanvas outgoingCanvas{outgoingFrame_, diagnosticSink_};
  apps_[indexOf(currentId_)]->render(outgoingCanvas, *this);
  incomingFrame_ = outgoingFrame_;
  transition_ = 0.0F;
  transitioning_ = true;
  swapped_ = false;
  emitDiagnostic({DiagnosticCategory::Runtime, DiagnosticCode::AppTransition,
                  "app transition", static_cast<std::int32_t>(indexOf(id))});
  return true;
}

void AppRuntime::emitDiagnostic(const DiagnosticEvent& event) const noexcept {
  if (diagnosticSink_) diagnosticSink_->emit(event);
}

void AppRuntime::update(Seconds dt, const InputFrame& input) noexcept {
  if (!begun_ || !apps_[indexOf(currentId_)]) return;
  if (!transitioning_ && input.longPressed && currentId_ != AppId::Launcher) {
    open(AppId::Launcher);
    return;
  }
  if (!transitioning_) {
    apps_[indexOf(currentId_)]->update(std::max(0.0F, dt), input, *this);
    return;
  }

  transition_ += std::max(0.0F, dt) / transitionDuration_;
  if (!swapped_ && transition_ >= 0.5F) {
    apps_[indexOf(currentId_)]->onExit();
    currentId_ = pendingId_;
    apps_[indexOf(currentId_)]->onEnter();
    incomingFrame_.clear(false);
    MonoCanvas incomingCanvas{incomingFrame_, diagnosticSink_};
    apps_[indexOf(currentId_)]->render(incomingCanvas, *this);
    swapped_ = true;
  }
  if (transition_ >= 1.0F) {
    transition_ = 1.0F;
    transitioning_ = false;
  }
}

void AppRuntime::render(MonoCanvas& canvas) noexcept {
  if (!begun_ || !apps_[indexOf(currentId_)]) {
    canvas.clear(false);
    canvas.text("RUNTIME: NO APP", canvas.width() / 2, canvas.height() / 2, 2,
                true, TextAlign::MiddleCenter);
    return;
  }
  if (transitioning_) {
    transitionStrategy_->compose(outgoingFrame_, incomingFrame_, canvas,
                                 transition_);
  } else {
    apps_[indexOf(currentId_)]->render(canvas, *this);
  }
}

}  // namespace cadenza
