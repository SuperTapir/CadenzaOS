#pragma once

#include <cstdint>

#include "cadenza/core/input.h"
#include "cadenza/core/timer_types.h"

namespace cadenza::system {

enum class TimerRequestResult : std::uint8_t {
  Accepted,
  InvalidState,
  InvalidDuration,
  InvalidOwner,
  NotOwner,
};

struct TimerServiceDiagnostics {
  std::uint32_t timestampRegressions = 0;
  std::uint32_t expirations = 0;
  std::uint32_t rejectedRequests = 0;
  TimerRequestResult lastResult = TimerRequestResult::Accepted;
};

class TimerService final {
 public:
  bool advanceTo(MonotonicMillis nowMs) noexcept;
  TimerRequestResult start(AppId owner, std::uint64_t durationMs) noexcept;
  TimerRequestResult pause(AppId owner) noexcept;
  TimerRequestResult resume(AppId owner) noexcept;
  TimerRequestResult setRemaining(AppId owner,
                                  std::uint64_t remainingMs) noexcept;
  TimerRequestResult acknowledge() noexcept;

  const TimerSnapshot& snapshot() const noexcept { return snapshot_; }
  const TimerServiceDiagnostics& diagnostics() const noexcept {
    return diagnostics_;
  }

 private:
  TimerRequestResult accept() noexcept;
  TimerRequestResult reject(TimerRequestResult result) noexcept;
  bool owns(AppId owner) const noexcept { return snapshot_.owner == owner; }
  void setDeadlineFromRemaining() noexcept;

  TimerSnapshot snapshot_{};
  TimerServiceDiagnostics diagnostics_{};
  MonotonicMillis nowMs_ = 0;
  MonotonicMillis deadlineMs_ = 0;
  bool timeInitialized_ = false;
};

}  // namespace cadenza::system
