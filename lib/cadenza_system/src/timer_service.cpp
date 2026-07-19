#include "cadenza/system/timer_service.h"

#include <limits>

namespace cadenza::system {

namespace {
bool validDuration(std::uint64_t durationMs) noexcept {
  return durationMs >= kTimerMinimumDurationMs &&
         durationMs <= kTimerMaximumDurationMs;
}
}  // namespace

TimerRequestResult TimerService::accept() noexcept {
  diagnostics_.lastResult = TimerRequestResult::Accepted;
  return TimerRequestResult::Accepted;
}

TimerRequestResult TimerService::reject(TimerRequestResult result) noexcept {
  ++diagnostics_.rejectedRequests;
  diagnostics_.lastResult = result;
  return result;
}

void TimerService::setDeadlineFromRemaining() noexcept {
  const MonotonicMillis maximum =
      std::numeric_limits<MonotonicMillis>::max();
  deadlineMs_ = snapshot_.remainingMs > maximum - nowMs_
                    ? maximum
                    : nowMs_ + snapshot_.remainingMs;
}

bool TimerService::advanceTo(MonotonicMillis nowMs) noexcept {
  if (!timeInitialized_) {
    nowMs_ = nowMs;
    timeInitialized_ = true;
  } else if (nowMs < nowMs_) {
    ++diagnostics_.timestampRegressions;
    return false;
  } else {
    nowMs_ = nowMs;
  }

  if (snapshot_.state != TimerState::Running) return false;
  if (nowMs_ < deadlineMs_) {
    snapshot_.remainingMs =
        static_cast<std::uint32_t>(deadlineMs_ - nowMs_);
    return false;
  }

  snapshot_.remainingMs = 0;
  snapshot_.state = TimerState::Expired;
  ++snapshot_.expirationGeneration;
  ++diagnostics_.expirations;
  return true;
}

TimerRequestResult TimerService::start(AppId owner,
                                       std::uint64_t durationMs) noexcept {
  if (!owner.valid()) return reject(TimerRequestResult::InvalidOwner);
  if (snapshot_.state != TimerState::Ready) {
    return reject(TimerRequestResult::InvalidState);
  }
  if (!validDuration(durationMs)) {
    return reject(TimerRequestResult::InvalidDuration);
  }
  snapshot_.configuredDurationMs = static_cast<std::uint32_t>(durationMs);
  snapshot_.remainingMs = static_cast<std::uint32_t>(durationMs);
  snapshot_.owner = owner;
  snapshot_.state = TimerState::Running;
  setDeadlineFromRemaining();
  return accept();
}

TimerRequestResult TimerService::pause(AppId owner) noexcept {
  if (snapshot_.state != TimerState::Running) {
    return reject(TimerRequestResult::InvalidState);
  }
  if (!owns(owner)) return reject(TimerRequestResult::NotOwner);
  snapshot_.state = TimerState::Paused;
  return accept();
}

TimerRequestResult TimerService::resume(AppId owner) noexcept {
  if (snapshot_.state != TimerState::Paused) {
    return reject(TimerRequestResult::InvalidState);
  }
  if (!owns(owner)) return reject(TimerRequestResult::NotOwner);
  snapshot_.state = TimerState::Running;
  setDeadlineFromRemaining();
  return accept();
}

TimerRequestResult TimerService::setRemaining(
    AppId owner, std::uint64_t remainingMs) noexcept {
  if (snapshot_.state != TimerState::Paused) {
    return reject(TimerRequestResult::InvalidState);
  }
  if (!owns(owner)) return reject(TimerRequestResult::NotOwner);
  if (!validDuration(remainingMs)) {
    return reject(TimerRequestResult::InvalidDuration);
  }
  snapshot_.configuredDurationMs = static_cast<std::uint32_t>(remainingMs);
  snapshot_.remainingMs = static_cast<std::uint32_t>(remainingMs);
  return accept();
}

TimerRequestResult TimerService::acknowledge() noexcept {
  if (snapshot_.state != TimerState::Expired) {
    return reject(TimerRequestResult::InvalidState);
  }
  snapshot_.state = TimerState::Ready;
  snapshot_.remainingMs = snapshot_.configuredDurationMs;
  snapshot_.owner = {};
  return accept();
}

}  // namespace cadenza::system
