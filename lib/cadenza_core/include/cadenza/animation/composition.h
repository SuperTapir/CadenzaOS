#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/core_types.h"

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
  template <typename Playable>
  bool add(Playable& playable) noexcept {
    if (size_ >= Capacity) return false;
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
  void update(Seconds delta) noexcept { seekTime(playhead_ + std::max(0.0F, delta)); }
  void reset() noexcept { seekTime(0.0F); }
  std::size_t size() const noexcept { return size_; }

 private:
  std::array<PlayableRef, Capacity> children_{};
  std::size_t size_ = 0;
  Seconds playhead_ = 0.0F;
};

template <std::size_t Capacity>
class Parallel {
 public:
  template <typename Playable>
  bool add(Playable& playable) noexcept {
    if (size_ >= Capacity) return false;
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
  void seekTime(Seconds time) noexcept {
    playhead_ = std::max(0.0F, std::min(duration(), time));
    for (std::size_t index = 0; index < size_; ++index) {
      children_[index].seekTime(
          std::min(children_[index].duration(), playhead_));
    }
  }
  void update(Seconds delta) noexcept { seekTime(playhead_ + std::max(0.0F, delta)); }
  void reset() noexcept { seekTime(0.0F); }
  std::size_t size() const noexcept { return size_; }

 private:
  std::array<PlayableRef, Capacity> children_{};
  std::size_t size_ = 0;
  Seconds playhead_ = 0.0F;
};

template <std::size_t Capacity>
class Timeline {
 public:
  template <typename Playable>
  bool add(Playable& playable, Seconds offset) noexcept {
    if (size_ >= Capacity || offset < 0.0F) return false;
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
  void seekTime(Seconds time) noexcept {
    playhead_ = std::max(0.0F, std::min(duration(), time));
    applyPlayhead();
  }
  void update(Seconds delta) noexcept {
    if (completed_ || duration() <= 0.0F) return;
    elapsed_ += std::max(0.0F, delta);
    const Seconds total = duration() * static_cast<float>(repeatCount_ + 1U);
    if (elapsed_ >= total) {
      elapsed_ = total;
      completed_ = true;
      playhead_ = reversed_ ? 0.0F : duration();
    } else {
      const Seconds local = std::fmod(elapsed_, duration());
      playhead_ = reversed_ ? duration() - local : local;
    }
    applyPlayhead();
  }
  void reset() noexcept {
    elapsed_ = 0.0F;
    completed_ = false;
    playhead_ = reversed_ ? duration() : 0.0F;
    applyPlayhead();
  }
  Seconds playhead() const noexcept { return playhead_; }
  bool completed() const noexcept { return completed_; }
  std::size_t size() const noexcept { return size_; }

 private:
  struct Entry {
    PlayableRef playable;
    Seconds offset = 0.0F;
  };
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
};

}  // namespace cadenza
