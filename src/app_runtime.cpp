#include "app_runtime.h"

#include <algorithm>

namespace {
constexpr float kTransitionSeconds = 0.32f;
uint8_t indexOf(AppId id) { return static_cast<uint8_t>(id); }
bool valid(AppId id) { return indexOf(id) < indexOf(AppId::Count); }
}

void AppRuntime::registerApp(AppId id, App& app, bool visibleInLauncher) {
  if (!valid(id)) return;
  apps_[indexOf(id)] = &app;
  visibleInLauncher_[indexOf(id)] = visibleInLauncher;
}

uint8_t AppRuntime::launcherAppCount() const {
  uint8_t count = 0;
  for (uint8_t i = 0; i < indexOf(AppId::Count); ++i) {
    if (apps_[i] && visibleInLauncher_[i]) ++count;
  }
  return count;
}

AppId AppRuntime::launcherAppAt(uint8_t position) const {
  for (uint8_t i = 0; i < indexOf(AppId::Count); ++i) {
    if (apps_[i] && visibleInLauncher_[i] && position-- == 0) return static_cast<AppId>(i);
  }
  return AppId::Launcher;
}

const char* AppRuntime::appName(AppId id) const {
  if (!valid(id)) return "INVALID";
  App* app = apps_[indexOf(id)];
  return app ? app->name() : "MISSING";
}

void AppRuntime::begin(AppId initial) {
  if (!valid(initial) || !apps_[indexOf(initial)]) return;
  currentId_ = initial;
  if (apps_[indexOf(currentId_)]) apps_[indexOf(currentId_)]->onEnter();
}

void AppRuntime::open(AppId id) {
  if (!valid(id) || id == currentId_ || transitioning_ || !apps_[indexOf(id)]) return;
  pendingId_ = id;
  transition_ = 0.0f;
  transitioning_ = true;
  swapped_ = false;
  Serial.printf("App transition: %s -> %s\n", apps_[indexOf(currentId_)]->name(),
                apps_[indexOf(pendingId_)]->name());
}

void AppRuntime::update(float dt, const InputFrame& input) {
  if (!apps_[indexOf(currentId_)]) return;
  if (!transitioning_ && input.longPressed && currentId_ != AppId::Launcher) {
    open(AppId::Launcher);
    return;
  }
  if (!transitioning_) {
    apps_[indexOf(currentId_)]->update(dt, input, *this);
    return;
  }

  transition_ += dt / kTransitionSeconds;
  if (!swapped_ && transition_ >= 0.5f) {
    apps_[indexOf(currentId_)]->onExit();
    currentId_ = pendingId_;
    apps_[indexOf(currentId_)]->onEnter();
    swapped_ = true;
  }
  if (transition_ >= 1.0f) transitioning_ = false;
}

void AppRuntime::render(MonoCanvas& canvas) {
  if (!apps_[indexOf(currentId_)]) {
    canvas.clear(false);
    canvas.text("RUNTIME: NO APP", canvas.width() / 2, canvas.height() / 2, 2, true,
                TextAlign::MiddleCenter);
    return;
  }
  apps_[indexOf(currentId_)]->render(canvas, *this);
  if (!transitioning_) return;

  const float shutter = transition_ < 0.5f ? transition_ * 2.0f : (1.0f - transition_) * 2.0f;
  const int bladeWidth = static_cast<int>((canvas.width() / 8.0f + 1) * shutter);
  for (int i = 0; i < 8; ++i) {
    const int x = i * canvas.width() / 8;
    if ((i & 1) == 0) canvas.fillRect(x, 0, bladeWidth, canvas.height(), true);
    else canvas.fillRect(x + canvas.width() / 8 - bladeWidth, 0, bladeWidth, canvas.height(), true);
  }
}
