#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/apps/apps.h"
#include "cadenza/core/app_runtime.h"
#include "cadenza/core/core_types.h"
#include "cadenza/core/input.h"
#include "cadenza/core/mono_canvas.h"
#include "cadenza/core/mono_framebuffer.h"
#include "cadenza/system/frame_coordinator.h"
#include "cadenza/host/headless_microphone.h"
#include "cadenza/host/headless_connectivity_adapter.h"

namespace cadenza::host {

std::uint64_t framebufferHash(const MonoFramebuffer& framebuffer) noexcept;
std::uint64_t pcmHash(const std::int16_t* samples,
                      std::size_t count) noexcept;

template <std::size_t Capacity>
class InputScript {
 public:
  struct Entry {
    FrameIndex frame = 0;
    InputFrame input;
  };

  bool add(FrameIndex frame, const InputFrame& input) noexcept {
    if (size_ >= Capacity || (size_ > 0 && frame <= entries_[size_ - 1].frame)) {
      return false;
    }
    entries_[size_++] = {frame, input};
    return true;
  }

  InputFrame inputAt(FrameIndex frame) const noexcept {
    for (std::size_t index = 0; index < size_; ++index) {
      if (entries_[index].frame == frame) return entries_[index].input;
      if (entries_[index].frame > frame) break;
    }
    return {};
  }

  std::size_t size() const noexcept { return size_; }
  constexpr std::size_t capacity() const noexcept { return Capacity; }

 private:
  std::array<Entry, Capacity> entries_{};
  std::size_t size_ = 0;
};

class DeterministicRunner {
 public:
  DeterministicRunner(AppRuntime& runtime, MonoCanvas& canvas,
                      MonoFramebuffer& framebuffer,
                      Seconds fixedDelta = 1.0F / 60.0F,
                      system::SystemServiceHost* services = nullptr) noexcept;

  void render() noexcept;
  void step(const InputFrame& input = {}) noexcept;
  void advance(Seconds delta, const InputFrame& input = {}) noexcept;
  void advanceAt(MonotonicMillis nowMs, Seconds presentationDelta,
                 const InputFrame& input = {}) noexcept;

  template <std::size_t Capacity>
  void runFrames(FrameIndex count,
                 const InputScript<Capacity>& script) noexcept {
    for (FrameIndex index = 0; index < count; ++index) {
      step(script.inputAt(frameIndex_));
    }
  }

  FrameIndex frameIndex() const noexcept { return frameIndex_; }
  Seconds simulationSeconds() const noexcept { return simulationSeconds_; }
  Seconds fixedDelta() const noexcept { return fixedDelta_; }
  std::uint64_t framebufferHash() const noexcept {
    return host::framebufferHash(framebuffer_);
  }

 private:
  AppRuntime& runtime_;
  MonoCanvas& canvas_;
  MonoFramebuffer& framebuffer_;
  system::SystemServiceHost* services_ = nullptr;
  Seconds fixedDelta_ = 1.0F / 60.0F;
  Seconds simulationSeconds_ = 0.0F;
  MonotonicMillis simulationNowMs_ = 0;
  double simulationSubmillis_ = 0.0;
  FrameIndex frameIndex_ = 0;
};

class HeadlessHost {
 public:
  explicit HeadlessHost(FramebufferProfile profile,
                        Seconds fixedDelta = 1.0F / 60.0F,
                        DiagnosticSink* diagnostics = nullptr) noexcept;

  void step(const InputFrame& input = {}) noexcept {
    connectivity_.pump();
    runner_.step(input);
    connectivity_.pump();
  }
  void advance(Seconds delta, const InputFrame& input = {}) noexcept {
    connectivity_.pump();
    runner_.advance(delta, input);
    connectivity_.pump();
  }
  void advanceAt(MonotonicMillis nowMs, Seconds presentationDelta,
                 const InputFrame& input = {}) noexcept {
    connectivity_.pump();
    runner_.advanceAt(nowMs, presentationDelta, input);
    connectivity_.pump();
  }
  void renderAudio(std::int16_t* samples, std::size_t count) noexcept {
    services_.renderAudio(samples, count);
  }
  std::uint64_t framebufferHash() const noexcept {
    return runner_.framebufferHash();
  }
  FrameIndex frameIndex() const noexcept { return runner_.frameIndex(); }
  Seconds simulationSeconds() const noexcept {
    return runner_.simulationSeconds();
  }
  AppRuntime& runtime() noexcept { return runtime_; }
  const AppRuntime& runtime() const noexcept { return runtime_; }
  system::SystemServiceHost& services() noexcept { return services_; }
  const system::SystemServiceHost& services() const noexcept {
    return services_;
  }
  HeadlessMicrophone& microphone() noexcept { return microphone_; }
  const HeadlessMicrophone& microphone() const noexcept { return microphone_; }
  HeadlessConnectivityAdapter& connectivity() noexcept { return connectivity_; }
  const HeadlessConnectivityAdapter& connectivity() const noexcept {
    return connectivity_;
  }
  MonoFramebuffer& framebuffer() noexcept { return framebuffer_; }
  const MonoFramebuffer& framebuffer() const noexcept { return framebuffer_; }

 private:
  LauncherApp launcher_;
  TimerApp timer_;
  MotionApp motion_;
  SettingsApp settings_;
  AnimationGalleryApp gallery_;
  system::SystemServiceHost services_;
  HeadlessConnectivityAdapter connectivity_;
  HeadlessMicrophone microphone_;
  AppRuntime runtime_;
  MonoFramebuffer framebuffer_;
  MonoCanvas canvas_;
  DeterministicRunner runner_;
};

}  // namespace cadenza::host
