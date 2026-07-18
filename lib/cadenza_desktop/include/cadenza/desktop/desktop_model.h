#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/app_runtime.h"
#include "cadenza/core/input_adapter.h"
#include "cadenza/core/mono_framebuffer.h"

namespace cadenza::desktop {

class DesktopDiagnosticLog final : public DiagnosticSink {
 public:
  void emit(const DiagnosticEvent& event) noexcept override {
    events_[write_] = event;
    write_ = (write_ + 1) % events_.size();
    if (count_ < events_.size()) ++count_;
    ++totalEvents_;
    if (event.code == DiagnosticCode::CapacityExceeded) ++capacityOverflows_;
  }

  const DiagnosticEvent* recent(std::size_t offset) const noexcept {
    if (offset >= count_) return nullptr;
    const std::size_t index =
        (write_ + events_.size() - 1 - offset) % events_.size();
    return &events_[index];
  }
  std::uint32_t totalEvents() const noexcept { return totalEvents_; }
  std::uint32_t capacityOverflows() const noexcept {
    return capacityOverflows_;
  }

 private:
  std::array<DiagnosticEvent, 32> events_{};
  std::size_t write_ = 0;
  std::size_t count_ = 0;
  std::uint32_t totalEvents_ = 0;
  std::uint32_t capacityOverflows_ = 0;
};

struct DesktopConfig {
  FramebufferProfile profile = FramebufferProfile::TEmbed;
  std::int16_t width = 320;
  std::int16_t height = 170;
  std::uint8_t scale = 2;
};

struct DevicePreviewLayout {
  std::int32_t logicalWidth = 0;
  std::int32_t logicalHeight = 0;
  std::int32_t windowWidth = 0;
  std::int32_t windowHeight = 0;
  std::int32_t screenX = 0;
  std::int32_t screenY = 0;
  std::int32_t screenWidth = 0;
  std::int32_t screenHeight = 0;
  std::int32_t scale = 1;

  static DevicePreviewLayout make(std::int32_t width, std::int32_t height,
                                  std::int32_t scale,
                                  bool framed) noexcept {
    const std::int32_t bezel = framed ? 24 : 0;
    return {width + bezel * 2, height + bezel * 2,
            (width + bezel * 2) * scale, (height + bezel * 2) * scale,
            bezel, bezel, width, height, scale};
  }

  bool toFramebuffer(std::int32_t windowX, std::int32_t windowY,
                     std::int32_t& framebufferX,
                     std::int32_t& framebufferY) const noexcept {
    if (scale <= 0) return false;
    const std::int32_t logicalX = windowX / scale - screenX;
    const std::int32_t logicalY = windowY / scale - screenY;
    if (logicalX < 0 || logicalY < 0 || logicalX >= screenWidth ||
        logicalY >= screenHeight) {
      return false;
    }
    framebufferX = logicalX;
    framebufferY = logicalY;
    return true;
  }
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
  std::size_t framebufferBytes = 0;
  std::size_t framebufferCapacity = 0;
  std::uint32_t capacityOverflows = 0;
  std::uint64_t heapEstimateBytes = 0;

  void toggle() noexcept { visible = !visible; }
  void update(float nextFps, float nextFrameMilliseconds, AppId nextApp,
              const InputFrame& nextInput) noexcept {
    fps = nextFps;
    frameMilliseconds = nextFrameMilliseconds;
    app = nextApp;
    input = nextInput;
  }
  void updateResources(std::size_t used, std::size_t capacity,
                       std::uint32_t overflows,
                       std::uint64_t heapEstimate) noexcept {
    framebufferBytes = used;
    framebufferCapacity = capacity;
    capacityOverflows = overflows;
    heapEstimateBytes = heapEstimate;
  }
};

}  // namespace cadenza::desktop
