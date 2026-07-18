#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/core_types.h"
#include "cadenza/core/diagnostics.h"

namespace cadenza {

class PlayableRef {
 public:
  PlayableRef() = default;

  template <typename Playable>
  explicit PlayableRef(Playable& playable) noexcept
      : object_(&playable),
        duration_([](const void* object) noexcept {
          return static_cast<const Playable*>(object)->totalDuration();
        }),
        seek_([](void* object, Seconds time) noexcept {
          static_cast<Playable*>(object)->seekTime(time);
        }) {}

  Seconds duration() const noexcept {
    return object_ ? duration_(object_) : 0.0F;
  }
  void seekTime(Seconds time) const noexcept {
    if (object_) seek_(object_, time);
  }
  bool valid() const noexcept { return object_ != nullptr; }

 private:
  void* object_ = nullptr;
  Seconds (*duration_)(const void*) noexcept = nullptr;
  void (*seek_)(void*, Seconds) noexcept = nullptr;
};

template <std::size_t Capacity>
class Sequence {
 public:
  explicit Sequence(DiagnosticSink* diagnostics = nullptr) noexcept
      : diagnostics_(diagnostics) {}

  template <typename Playable>
  bool add(Playable& playable) noexcept {
    if (size_ >= Capacity) {
      emitCapacity("sequence is full");
      return false;
    }
    children_[size_++] = PlayableRef{playable};
    return true;
  }
  Seconds duration() const noexcept {
    Seconds result = 0.0F;
    for (std::size_t index = 0; index < size_; ++index) {
      result += children_[index].duration();
    }
    return result;
  }
  Seconds totalDuration() const noexcept { return duration(); }
  Sequence& onComplete(CompletionCallback callback, void* context) noexcept {
    callback_ = callback;
    callbackContext_ = context;
    return *this;
  }
  void seekTime(Seconds time) noexcept {
    playhead_ = std::max(0.0F, std::min(duration(), time));
    Seconds offset = 0.0F;
    for (std::size_t index = 0; index < size_; ++index) {
      const Seconds childDuration = children_[index].duration();
      children_[index].seekTime(
          std::max(0.0F, std::min(childDuration, playhead_ - offset)));
      offset += childDuration;
    }
  }
  void update(Seconds delta) noexcept {
    if (completed_) return;
    seekTime(playhead_ + std::max(0.0F, delta));
    if (playhead_ >= duration()) {
      completed_ = true;
      notifyCompletion();
    }
  }
  void reset() noexcept {
    completed_ = false;
    callbackSent_ = false;
    seekTime(0.0F);
  }
  std::size_t size() const noexcept { return size_; }
  bool completed() const noexcept { return completed_; }

 private:
  void emitCapacity(const char* message) const noexcept {
    if (diagnostics_) {
      diagnostics_->emit({DiagnosticCategory::Capacity,
                          DiagnosticCode::CapacityExceeded, message,
                          static_cast<std::int32_t>(Capacity)});
    }
  }
  void notifyCompletion() noexcept {
    if (!callbackSent_ && callback_) callback_(callbackContext_);
    callbackSent_ = true;
  }
  std::array<PlayableRef, Capacity> children_{};
  std::size_t size_ = 0;
  Seconds playhead_ = 0.0F;
  DiagnosticSink* diagnostics_ = nullptr;
  CompletionCallback callback_ = nullptr;
  void* callbackContext_ = nullptr;
  bool completed_ = false;
  bool callbackSent_ = false;
};

template <std::size_t Capacity>
class Parallel {
 public:
  explicit Parallel(DiagnosticSink* diagnostics = nullptr) noexcept
      : diagnostics_(diagnostics) {}

  template <typename Playable>
  bool add(Playable& playable) noexcept {
    if (size_ >= Capacity) {
      emitCapacity("parallel group is full");
      return false;
    }
    children_[size_++] = PlayableRef{playable};
    return true;
  }
  Seconds duration() const noexcept {
    Seconds result = 0.0F;
    for (std::size_t index = 0; index < size_; ++index) {
      result = std::max(result, children_[index].duration());
    }
    return result;
  }
  Seconds totalDuration() const noexcept { return duration(); }
  Parallel& onComplete(CompletionCallback callback, void* context) noexcept {
    callback_ = callback;
    callbackContext_ = context;
    return *this;
  }
  void seekTime(Seconds time) noexcept {
    playhead_ = std::max(0.0F, std::min(duration(), time));
    for (std::size_t index = 0; index < size_; ++index) {
      children_[index].seekTime(
          std::min(children_[index].duration(), playhead_));
    }
  }
  void update(Seconds delta) noexcept {
    if (completed_) return;
    seekTime(playhead_ + std::max(0.0F, delta));
    if (playhead_ >= duration()) {
      completed_ = true;
      notifyCompletion();
    }
  }
  void reset() noexcept {
    completed_ = false;
    callbackSent_ = false;
    seekTime(0.0F);
  }
  std::size_t size() const noexcept { return size_; }
  bool completed() const noexcept { return completed_; }

 private:
  void emitCapacity(const char* message) const noexcept {
    if (diagnostics_) {
      diagnostics_->emit({DiagnosticCategory::Capacity,
                          DiagnosticCode::CapacityExceeded, message,
                          static_cast<std::int32_t>(Capacity)});
    }
  }
  void notifyCompletion() noexcept {
    if (!callbackSent_ && callback_) callback_(callbackContext_);
    callbackSent_ = true;
  }
  std::array<PlayableRef, Capacity> children_{};
  std::size_t size_ = 0;
  Seconds playhead_ = 0.0F;
  DiagnosticSink* diagnostics_ = nullptr;
  CompletionCallback callback_ = nullptr;
  void* callbackContext_ = nullptr;
  bool completed_ = false;
  bool callbackSent_ = false;
};

template <std::size_t Capacity>
class Timeline {
 public:
  explicit Timeline(DiagnosticSink* diagnostics = nullptr) noexcept
      : diagnostics_(diagnostics) {}

  template <typename Playable>
  bool add(Playable& playable, Seconds offset) noexcept {
    if (size_ >= Capacity) {
      emit(DiagnosticCategory::Capacity, DiagnosticCode::CapacityExceeded,
           "timeline is full");
      return false;
    }
    if (offset < 0.0F) {
      emit(DiagnosticCategory::Animation, DiagnosticCode::InvalidOperation,
           "negative timeline offset");
      return false;
    }
    entries_[size_++] = {PlayableRef{playable}, offset};
    return true;
  }
  Timeline& setReversed(bool reversed) noexcept {
    reversed_ = reversed;
    return *this;
  }
  Timeline& setRepeat(std::uint32_t count) noexcept {
    repeatCount_ = count;
    return *this;
  }
  Seconds duration() const noexcept {
    Seconds result = 0.0F;
    for (std::size_t index = 0; index < size_; ++index) {
      result = std::max(result,
                        entries_[index].offset + entries_[index].playable.duration());
    }
    return result;
  }
  Seconds totalDuration() const noexcept { return duration(); }
  Timeline& onComplete(CompletionCallback callback, void* context) noexcept {
    callback_ = callback;
    callbackContext_ = context;
    return *this;
  }
  void seekTime(Seconds time) noexcept {
    playhead_ = std::max(0.0F, std::min(duration(), time));
    elapsed_ = reversed_ ? duration() - playhead_ : playhead_;
    completed_ = false;
    applyPlayhead();
  }
  void seek(float progress) noexcept {
    const float clamped = std::max(0.0F, std::min(1.0F, progress));
    seekTime(duration() * clamped);
  }
  void update(Seconds delta) noexcept {
    if (paused_ || completed_ || duration() <= 0.0F) return;
    elapsed_ += std::max(0.0F, delta);
    const Seconds total = duration() * static_cast<float>(repeatCount_ + 1U);
    if (elapsed_ >= total) {
      elapsed_ = total;
      completed_ = true;
      playhead_ = reversed_ ? 0.0F : duration();
      notifyCompletion();
    } else {
      const Seconds local = std::fmod(elapsed_, duration());
      playhead_ = reversed_ ? duration() - local : local;
    }
    applyPlayhead();
  }
  void reset() noexcept {
    elapsed_ = 0.0F;
    completed_ = false;
    callbackSent_ = false;
    playhead_ = reversed_ ? duration() : 0.0F;
    applyPlayhead();
  }
  Seconds playhead() const noexcept { return playhead_; }
  float progress() const noexcept {
    return duration() <= 0.0F ? 0.0F : playhead_ / duration();
  }
  void pause() noexcept { paused_ = true; }
  void resume() noexcept { paused_ = false; }
  bool paused() const noexcept { return paused_; }
  bool completed() const noexcept { return completed_; }
  std::size_t size() const noexcept { return size_; }

 private:
  struct Entry {
    PlayableRef playable;
    Seconds offset = 0.0F;
  };
  void emit(DiagnosticCategory category, DiagnosticCode code,
            const char* message) const noexcept {
    if (diagnostics_) {
      diagnostics_->emit({category, code, message,
                          static_cast<std::int32_t>(Capacity)});
    }
  }
  void notifyCompletion() noexcept {
    if (!callbackSent_ && callback_) callback_(callbackContext_);
    callbackSent_ = true;
  }
  void applyPlayhead() noexcept {
    for (std::size_t index = 0; index < size_; ++index) {
      const Seconds local = playhead_ - entries_[index].offset;
      entries_[index].playable.seekTime(std::max(
          0.0F, std::min(entries_[index].playable.duration(), local)));
    }
  }

  std::array<Entry, Capacity> entries_{};
  std::size_t size_ = 0;
  Seconds elapsed_ = 0.0F;
  Seconds playhead_ = 0.0F;
  std::uint32_t repeatCount_ = 0;
  bool reversed_ = false;
  bool completed_ = false;
  bool paused_ = false;
  DiagnosticSink* diagnostics_ = nullptr;
  CompletionCallback callback_ = nullptr;
  void* callbackContext_ = nullptr;
  bool callbackSent_ = false;
};

}  // namespace cadenza
