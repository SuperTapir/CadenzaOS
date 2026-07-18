#pragma once

#include <Arduino.h>

#include "input.h"
#include "mono_canvas.h"
#include "cadenza/core/diagnostics.h"

enum class AppId : uint8_t { Launcher, Clock, Motion, Settings, Count };

class AppRuntime;

class App {
 public:
  virtual ~App() = default;
  virtual const char* name() const = 0;
  virtual void onEnter() {}
  virtual void onExit() {}
  virtual void update(float dt, const InputFrame& input, AppRuntime& runtime) = 0;
  virtual void render(MonoCanvas& canvas, const AppRuntime& runtime) = 0;
};

class AppRuntime {
 public:
  void registerApp(AppId id, App& app, bool visibleInLauncher = true);
  void begin(AppId initial);
  void update(float dt, const InputFrame& input);
  void render(MonoCanvas& canvas);
  void open(AppId id);
  void setDiagnosticSink(cadenza::DiagnosticSink* sink) { diagnosticSink_ = sink; }
  void emitDiagnostic(const cadenza::DiagnosticEvent& event) const;
  AppId currentId() const { return currentId_; }
  uint8_t launcherAppCount() const;
  AppId launcherAppAt(uint8_t position) const;
  const char* appName(AppId id) const;

 private:
  App* apps_[static_cast<uint8_t>(AppId::Count)] = {};
  bool visibleInLauncher_[static_cast<uint8_t>(AppId::Count)] = {};
  AppId currentId_ = AppId::Launcher;
  AppId pendingId_ = AppId::Launcher;
  float transition_ = 0.0f;
  bool transitioning_ = false;
  bool swapped_ = false;
  cadenza::DiagnosticSink* diagnosticSink_ = nullptr;
};
