#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/app_runtime.h"
#include "cadenza/core/input_adapter.h"
#include "cadenza/core/mono_framebuffer.h"

namespace cadenza::desktop {

struct DesktopConfig {
  FramebufferProfile profile = FramebufferProfile::TEmbed;
  std::int16_t width = 320;
  std::int16_t height = 170;
  std::uint8_t scale = 2;
};

bool configure(DesktopConfig& output, const char* profile,
               int scale) noexcept;

enum class DesktopKey : std::uint8_t {
  Left,
  Right,
  Space,
  Enter,
  Unknown,
};

class DesktopInputMapper final : public RawInputSource {
 public:
  bool wheel(float deltaY, MonotonicMillis timestampMs) noexcept;
  bool key(DesktopKey key, bool down, bool repeat,
           MonotonicMillis timestampMs) noexcept;
  bool poll(RawInputEvent& event) noexcept override;

 private:
  static constexpr std::size_t kCapacity = 32;
  bool enqueue(RawInputType type, std::int32_t value,
               MonotonicMillis timestampMs) noexcept;
  bool anyButtonDown() const noexcept { return spaceDown_ || enterDown_; }

  std::array<RawInputEvent, kCapacity> queue_{};
  std::size_t read_ = 0;
  std::size_t write_ = 0;
  bool spaceDown_ = false;
  bool enterDown_ = false;
};

struct OverlayState {
  bool visible = false;
  float fps = 0.0F;
  float frameMilliseconds = 0.0F;
  AppId app = AppId::Launcher;
  InputFrame input;

  void toggle() noexcept { visible = !visible; }
  void update(float nextFps, float nextFrameMilliseconds, AppId nextApp,
              const InputFrame& nextInput) noexcept {
    fps = nextFps;
    frameMilliseconds = nextFrameMilliseconds;
    app = nextApp;
    input = nextInput;
  }
};

}  // namespace cadenza::desktop
