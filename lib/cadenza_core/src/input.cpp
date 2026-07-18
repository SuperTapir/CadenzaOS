#include "cadenza/core/input.h"

#include <algorithm>
#include <limits>

namespace cadenza {
namespace {
std::uint32_t saturatedDuration(MonotonicMillis start,
                                MonotonicMillis end) noexcept {
  if (end <= start) {
    return 0;
  }
  const auto duration = end - start;
  return duration > std::numeric_limits<std::uint32_t>::max()
             ? std::numeric_limits<std::uint32_t>::max()
             : static_cast<std::uint32_t>(duration);
}
}  // namespace

void InputReducer::push(const RawInputEvent& event) noexcept {
  if (event.timestampMs < nowMs_) {
    return;
  }
  advanceTo(event.timestampMs);

  if (event.type == RawInputType::Turn) {
    accumulateTurn(event.value);
    return;
  }

  const bool buttonDown = event.type == RawInputType::ButtonDown;
  if (buttonDown != rawButtonDown_) {
    rawButtonDown_ = buttonDown;
    rawChangedAtMs_ = event.timestampMs;
  }
}

void InputReducer::advanceTo(MonotonicMillis nowMs) noexcept {
  if (nowMs < nowMs_) {
    return;
  }

  if (rawButtonDown_ != stableButtonDown_ &&
      nowMs - rawChangedAtMs_ >= config_.debounceMs) {
    const MonotonicMillis stableAt = rawChangedAtMs_ + config_.debounceMs;
    updateHeld(stableAt);
    stableButtonDown_ = rawButtonDown_;

    if (stableButtonDown_) {
      pressedAtMs_ = stableAt;
      longPressSent_ = false;
      pending_.pressed = true;
      pending_.heldMs = 0;
    } else {
      pending_.released = true;
      pending_.clicked = !longPressSent_;
      pending_.heldMs = 0;
    }
  }

  updateHeld(nowMs);
  nowMs_ = nowMs;
}

InputFrame InputReducer::takeFrame() noexcept {
  const InputFrame frame = pending_;
  pending_.turn = 0;
  pending_.pressed = false;
  pending_.released = false;
  pending_.clicked = false;
  pending_.longPressed = false;
  if (!stableButtonDown_) {
    pending_.heldMs = 0;
  }
  return frame;
}

void InputReducer::updateHeld(MonotonicMillis atMs) noexcept {
  if (!stableButtonDown_) {
    return;
  }

  pending_.heldMs = saturatedDuration(pressedAtMs_, atMs);
  if (!longPressSent_ && pending_.heldMs >= config_.longPressMs) {
    longPressSent_ = true;
    pending_.longPressed = true;
  }
}

void InputReducer::accumulateTurn(std::int32_t delta) noexcept {
  const std::int64_t sum = static_cast<std::int64_t>(pending_.turn) + delta;
  const auto minimum = std::numeric_limits<std::int16_t>::min();
  const auto maximum = std::numeric_limits<std::int16_t>::max();
  pending_.turn = static_cast<std::int16_t>(
      std::max<std::int64_t>(minimum, std::min<std::int64_t>(maximum, sum)));
}

}  // namespace cadenza
