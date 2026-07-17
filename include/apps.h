#pragma once

#include "app_runtime.h"

class LauncherApp : public App {
 public:
  const char* name() const override { return "Launcher"; }
  void onEnter() override;
  void update(float dt, const InputFrame& input, AppRuntime& runtime) override;
  void render(MonoCanvas& canvas, const AppRuntime& runtime) override;

 private:
  int selected_ = 0;
  float position_ = 0.0f;
  float pulse_ = 0.0f;
  float time_ = 0.0f;
};

class ClockApp : public App {
 public:
  const char* name() const override { return "Clock"; }
  void onEnter() override;
  void update(float dt, const InputFrame& input, AppRuntime& runtime) override;
  void render(MonoCanvas& canvas, const AppRuntime& runtime) override;

 private:
  bool running_ = true;
  float elapsed_ = 0.0f;
  float phase_ = 0.0f;
};

class MotionApp : public App {
 public:
  const char* name() const override { return "Motion"; }
  void onEnter() override;
  void update(float dt, const InputFrame& input, AppRuntime& runtime) override;
  void render(MonoCanvas& canvas, const AppRuntime& runtime) override;

 private:
  float target_ = 0.5f;
  float x_ = 0.5f;
  float velocity_ = 0.0f;
  float time_ = 0.0f;
};

class SettingsApp : public App {
 public:
  const char* name() const override { return "Settings"; }
  void update(float dt, const InputFrame& input, AppRuntime& runtime) override;
  void render(MonoCanvas& canvas, const AppRuntime& runtime) override;

 private:
  int selected_ = 0;
  bool energetic_ = true;
  float time_ = 0.0f;
};
