#pragma once

#include <cstdint>

#include "cadenza/core/app_id.h"

namespace cadenza {

inline constexpr std::uint64_t kTimerMinuteMs = 60'000;
inline constexpr std::uint64_t kTimerDefaultDurationMs =
    10 * kTimerMinuteMs;
inline constexpr std::uint64_t kTimerMinimumDurationMs = kTimerMinuteMs;
inline constexpr std::uint64_t kTimerMaximumDurationMs =
    60 * kTimerMinuteMs;

enum class TimerState : std::uint8_t {
  Ready,
  Running,
  Paused,
  Expired,
};

struct TimerSnapshot {
  TimerState state = TimerState::Ready;
  std::uint32_t configuredDurationMs =
      static_cast<std::uint32_t>(kTimerDefaultDurationMs);
  std::uint32_t remainingMs =
      static_cast<std::uint32_t>(kTimerDefaultDurationMs);
  AppId owner{};
  std::uint32_t expirationGeneration = 0;

  friend constexpr bool operator==(const TimerSnapshot& lhs,
                                   const TimerSnapshot& rhs) noexcept {
    return lhs.state == rhs.state &&
           lhs.configuredDurationMs == rhs.configuredDurationMs &&
           lhs.remainingMs == rhs.remainingMs && lhs.owner == rhs.owner &&
           lhs.expirationGeneration == rhs.expirationGeneration;
  }
  friend constexpr bool operator!=(const TimerSnapshot& lhs,
                                   const TimerSnapshot& rhs) noexcept {
    return !(lhs == rhs);
  }
};

}  // namespace cadenza
