#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/apps.h"
#include "cadenza/core/core_types.h"
#include "cadenza/core/input.h"
#include "cadenza/core/mono_canvas.h"
#include "cadenza/core/mono_framebuffer.h"

namespace cadenza::host {

std::uint64_t framebufferHash(const MonoFramebuffer& framebuffer) noexcept;

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
                      Seconds fixedDelta = 1.0F / 60.0F) noexcept;

  void render() noexcept;
  void step(const InputFrame& input = {}) noexcept;
  void advance(Seconds delta, const InputFrame& input = {}) noexcept;

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
  Seconds fixedDelta_ = 1.0F / 60.0F;
  Seconds simulationSeconds_ = 0.0F;
  FrameIndex frameIndex_ = 0;
};

class HeadlessHost {
 public:
  explicit HeadlessHost(FramebufferProfile profile,
                        Seconds fixedDelta = 1.0F / 60.0F,
                        DiagnosticSink* diagnostics = nullptr) noexcept;

  void step(const InputFrame& input = {}) noexcept { runner_.step(input); }
  void advance(Seconds delta, const InputFrame& input = {}) noexcept {
    runner_.advance(delta, input);
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
  MonoFramebuffer& framebuffer() noexcept { return framebuffer_; }
  const MonoFramebuffer& framebuffer() const noexcept { return framebuffer_; }

 private:
  LauncherApp launcher_;
  ClockApp clock_;
  MotionApp motion_;
  SettingsApp settings_;
  AnimationGalleryApp gallery_;
  AppRuntime runtime_;
  MonoFramebuffer framebuffer_;
  MonoCanvas canvas_;
  DeterministicRunner runner_;
};

}  // namespace cadenza::host
