#pragma once

#include "cadenza/core/app_runtime.h"

namespace cadenza {

class LauncherApp final : public App {
 public:
  const char* name() const noexcept override { return "Launcher"; }
  void onEnter() noexcept override;
  void update(Seconds dt, const InputFrame& input,
              AppRuntime& runtime) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRuntime& runtime) noexcept override;

 private:
  int selected_ = 0;
  float position_ = 0.0F;
  float pulse_ = 0.0F;
  float time_ = 0.0F;
};

class ClockApp final : public App {
 public:
  const char* name() const noexcept override { return "Clock"; }
  void onEnter() noexcept override;
  void update(Seconds dt, const InputFrame& input,
              AppRuntime& runtime) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRuntime& runtime) noexcept override;

 private:
  bool running_ = true;
  float elapsed_ = 0.0F;
  float phase_ = 0.0F;
};

class MotionApp final : public App {
 public:
  const char* name() const noexcept override { return "Motion"; }
  void onEnter() noexcept override;
  void update(Seconds dt, const InputFrame& input,
              AppRuntime& runtime) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRuntime& runtime) noexcept override;

 private:
  float target_ = 0.5F;
  float x_ = 0.5F;
  float velocity_ = 0.0F;
  float time_ = 0.0F;
};

class SettingsApp final : public App {
 public:
  const char* name() const noexcept override { return "Settings"; }
  void update(Seconds dt, const InputFrame& input,
              AppRuntime& runtime) noexcept override;
  void render(MonoCanvas& canvas,
              const AppRuntime& runtime) noexcept override;

 private:
  int selected_ = 0;
  bool energetic_ = true;
  float time_ = 0.0F;
};

}  // namespace cadenza
