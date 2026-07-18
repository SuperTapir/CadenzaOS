#pragma once

#include <cstdint>

namespace cadenza {

using MonotonicMillis = std::uint64_t;

enum class RawInputType : std::uint8_t {
  Turn,
  ButtonDown,
  ButtonUp,
};

struct RawInputEvent {
  RawInputType type = RawInputType::Turn;
  MonotonicMillis timestampMs = 0;
  std::int32_t value = 0;
};

struct InputConfig {
  std::uint32_t debounceMs = 22;
  std::uint32_t longPressMs = 650;
};

struct InputFrame {
  std::int16_t turn = 0;
  bool pressed = false;
  bool released = false;
  bool clicked = false;
  bool longPressed = false;
  std::uint32_t heldMs = 0;
};

class InputReducer {
 public:
  explicit InputReducer(InputConfig config = {}) noexcept : config_(config) {}

  void push(const RawInputEvent& event) noexcept;
  void advanceTo(MonotonicMillis nowMs) noexcept;
  InputFrame takeFrame() noexcept;

  bool buttonDown() const noexcept { return stableButtonDown_; }
  MonotonicMillis nowMs() const noexcept { return nowMs_; }

 private:
  void updateHeld(MonotonicMillis atMs) noexcept;
  void accumulateTurn(std::int32_t delta) noexcept;

  InputConfig config_;
  MonotonicMillis nowMs_ = 0;
  MonotonicMillis rawChangedAtMs_ = 0;
  MonotonicMillis pressedAtMs_ = 0;
  bool rawButtonDown_ = false;
  bool stableButtonDown_ = false;
  bool longPressSent_ = false;
  InputFrame pending_;
};

}  // namespace cadenza
