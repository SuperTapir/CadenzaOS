#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

#include "cadenza/animation/easing.h"
#include "cadenza/core/core_types.h"

namespace cadenza {

enum class TweenState : std::uint8_t {
  Delayed,
  Running,
  Completed,
};

template <typename T>
class Tween {
 public:
  Tween(T from, T to, Seconds duration,
        Easing easing = Easing::Linear) noexcept
      : from_(from),
        to_(to),
        duration_(std::max(0.0F, duration)),
        easing_(easing) {
    reset();
  }

  Tween& setDelay(Seconds delay) noexcept {
    delay_ = std::max(0.0F, delay);
    reset();
    return *this;
  }
  Tween& setRepeat(std::uint32_t count,
                   Seconds repeatDelay = 0.0F) noexcept {
    infinite_ = false;
    repeatCount_ = count;
    repeatDelay_ = std::max(0.0F, repeatDelay);
    reset();
    return *this;
  }
  Tween& setRepeatForever(Seconds repeatDelay = 0.0F) noexcept {
    infinite_ = true;
    repeatCount_ = 0;
    repeatDelay_ = std::max(0.0F, repeatDelay);
    reset();
    return *this;
  }
  Tween& setYoyo(bool enabled) noexcept {
    yoyo_ = enabled;
    reset();
    return *this;
  }
  Tween& setReversed(bool reversed) noexcept {
    initiallyReversed_ = reversed;
    reset();
    return *this;
  }
  Tween& onComplete(CompletionCallback callback, void* context) noexcept {
    callback_ = callback;
    callbackContext_ = context;
    return *this;
  }

  void reset() noexcept {
    legElapsed_ = 0.0F;
    delayRemaining_ = delay_;
    iteration_ = 0;
    reversed_ = initiallyReversed_;
    repeatPending_ = false;
    completed_ = false;
    callbackSent_ = false;
    infiniteElapsed_ = 0.0;
  }

  void update(Seconds delta) noexcept {
    if (infinite_) {
      infiniteElapsed_ += static_cast<double>(std::max(0.0F, delta));
      applyInfiniteTime();
      return;
    }
    Seconds remaining = std::max(0.0F, delta);
    while (remaining > 0.0F && !completed_) {
      if (delayRemaining_ > 0.0F) {
        if (remaining < delayRemaining_) {
          delayRemaining_ -= remaining;
          return;
        }
        remaining -= delayRemaining_;
        delayRemaining_ = 0.0F;
        if (repeatPending_) beginRepeatedLeg();
      }

      if (duration_ <= 0.0F) {
        legElapsed_ = duration_;
      } else {
        const Seconds legRemaining = duration_ - legElapsed_;
        if (remaining < legRemaining) {
          legElapsed_ += remaining;
          return;
        }
        remaining -= legRemaining;
        legElapsed_ = duration_;
      }

      if (iteration_ >= repeatCount_) {
        completed_ = true;
        notifyCompletion();
        return;
      }
      ++iteration_;
      repeatPending_ = true;
      delayRemaining_ = repeatDelay_;
      if (delayRemaining_ <= 0.0F) beginRepeatedLeg();
    }
  }

  T value() const noexcept {
    const float raw = duration_ <= 0.0F
                          ? (completed_ ? 1.0F : 0.0F)
                          : std::max(0.0F, std::min(1.0F,
                                                   legElapsed_ / duration_));
    const float eased = ease(easing_, raw);
    const float amount = reversed_ ? 1.0F - eased : eased;
    return static_cast<T>(from_ + (to_ - from_) * amount);
  }

  float progress() const noexcept {
    const float raw = duration_ <= 0.0F
                          ? (completed_ ? 1.0F : 0.0F)
                          : std::max(0.0F, std::min(1.0F,
                                                   legElapsed_ / duration_));
    return reversed_ ? 1.0F - raw : raw;
  }

  void seek(float progress) noexcept {
    const float canonical = std::max(0.0F, std::min(1.0F, progress));
    const float raw = reversed_ ? 1.0F - canonical : canonical;
    legElapsed_ = duration_ * raw;
    delayRemaining_ = 0.0F;
    repeatPending_ = false;
    if (infinite_) {
      infiniteElapsed_ = static_cast<double>(delay_) + legElapsed_;
    }
  }

  TweenState state() const noexcept {
    if (completed_) return TweenState::Completed;
    if (delayRemaining_ > 0.0F) return TweenState::Delayed;
    return TweenState::Running;
  }
  Seconds duration() const noexcept { return duration_; }
  Seconds totalDuration() const noexcept {
    if (infinite_) return std::numeric_limits<Seconds>::infinity();
    return delay_ + duration_ * static_cast<float>(repeatCount_ + 1U) +
           repeatDelay_ * static_cast<float>(repeatCount_);
  }

  void seekTime(Seconds time) noexcept {
    if (infinite_) {
      infiniteElapsed_ = static_cast<double>(std::max(0.0F, time));
      applyInfiniteTime();
      return;
    }
    const Seconds clamped =
        std::max(0.0F, std::min(totalDuration(), time));
    if (clamped <= delay_) {
      reversed_ = initiallyReversed_;
      legElapsed_ = 0.0F;
      return;
    }
    Seconds local = clamped - delay_;
    const Seconds cycle = duration_ + repeatDelay_;
    std::uint32_t traversal = 0;
    Seconds within = local;
    if (cycle > 0.0F) {
      traversal = static_cast<std::uint32_t>(local / cycle);
      if (traversal > repeatCount_) traversal = repeatCount_;
      within = local - cycle * static_cast<float>(traversal);
    }
    if (clamped >= totalDuration()) {
      traversal = repeatCount_;
      within = duration_;
    }
    reversed_ = initiallyReversed_ ^ (yoyo_ && (traversal & 1U));
    legElapsed_ = std::min(duration_, within);
    iteration_ = traversal;
    delayRemaining_ = 0.0F;
    repeatPending_ = false;
  }
  bool infinite() const noexcept { return infinite_; }

 private:
  void applyInfiniteTime() noexcept {
    completed_ = false;
    callbackSent_ = false;
    if (infiniteElapsed_ < static_cast<double>(delay_)) {
      reversed_ = initiallyReversed_;
      legElapsed_ = 0.0F;
      delayRemaining_ = static_cast<Seconds>(
          static_cast<double>(delay_) - infiniteElapsed_);
      repeatPending_ = false;
      return;
    }

    const double local = infiniteElapsed_ - static_cast<double>(delay_);
    if (duration_ <= 0.0F) {
      std::uint64_t traversal = 0;
      double within = 0.0;
      if (repeatDelay_ > 0.0F) {
        traversal = static_cast<std::uint64_t>(
            std::floor(local / static_cast<double>(repeatDelay_)));
        within = std::fmod(local, static_cast<double>(repeatDelay_));
      }
      reversed_ = initiallyReversed_ ^ (yoyo_ && (traversal & 1U));
      legElapsed_ = 0.0F;
      repeatPending_ = within > 0.0;
      delayRemaining_ = repeatPending_
                            ? static_cast<Seconds>(repeatDelay_ - within)
                            : 0.0F;
      return;
    }

    const double cycle = static_cast<double>(duration_ + repeatDelay_);
    const std::uint64_t traversal = static_cast<std::uint64_t>(
        std::floor(local / cycle));
    const double within = std::fmod(local, cycle);
    reversed_ = initiallyReversed_ ^ (yoyo_ && (traversal & 1U));
    iteration_ = static_cast<std::uint32_t>(traversal);
    if (within < static_cast<double>(duration_)) {
      legElapsed_ = static_cast<Seconds>(within);
      delayRemaining_ = 0.0F;
      repeatPending_ = false;
    } else {
      legElapsed_ = duration_;
      delayRemaining_ = static_cast<Seconds>(cycle - within);
      repeatPending_ = true;
    }
  }

  void beginRepeatedLeg() noexcept {
    if (yoyo_) reversed_ = !reversed_;
    legElapsed_ = 0.0F;
    repeatPending_ = false;
  }
  void notifyCompletion() noexcept {
    if (!callbackSent_ && callback_) callback_(callbackContext_);
    callbackSent_ = true;
  }

  T from_;
  T to_;
  Seconds duration_ = 0.0F;
  Easing easing_ = Easing::Linear;
  Seconds delay_ = 0.0F;
  Seconds repeatDelay_ = 0.0F;
  Seconds legElapsed_ = 0.0F;
  Seconds delayRemaining_ = 0.0F;
  std::uint32_t repeatCount_ = 0;
  std::uint32_t iteration_ = 0;
  double infiniteElapsed_ = 0.0;
  CompletionCallback callback_ = nullptr;
  void* callbackContext_ = nullptr;
  bool yoyo_ = false;
  bool initiallyReversed_ = false;
  bool reversed_ = false;
  bool repeatPending_ = false;
  bool completed_ = false;
  bool callbackSent_ = false;
  bool infinite_ = false;
};

}  // namespace cadenza
