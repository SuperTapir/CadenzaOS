#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/core_types.h"
#include "cadenza/core/sprite_atlas.h"

namespace cadenza {

enum class FramePlayback : std::uint8_t {
  Once,
  Loop,
  PingPong,
};

class FrameAnimation {
 public:
  template <std::size_t Capacity>
  FrameAnimation(const SpriteAtlas<Capacity>& atlas, std::size_t firstFrame,
                 std::size_t frameCount, Seconds frameDuration,
                 FramePlayback playback = FramePlayback::Loop) noexcept
      : atlas_(&atlas),
        frameAt_(&atlasFrame<Capacity>),
        atlasSize_(atlas.size()),
        firstFrame_(firstFrame),
        frameCount_(frameCount),
        frameDuration_(frameDuration),
        playback_(playback) {}

  bool valid() const noexcept {
    return atlas_ != nullptr && frameAt_ != nullptr && frameCount_ > 0 &&
           frameDuration_ > 0.0F && firstFrame_ <= atlasSize_ &&
           frameCount_ <= atlasSize_ - firstFrame_;
  }

  void reset() noexcept {
    elapsed_ = 0.0F;
    completed_ = false;
  }

  void update(Seconds delta) noexcept {
    if (!valid() || delta <= 0.0F || completed_) return;
    elapsed_ += delta;
    if (playback_ == FramePlayback::Once &&
        elapsed_ >= onceDuration()) {
      elapsed_ = onceDuration();
      completed_ = true;
    } else if (playback_ != FramePlayback::Once) {
      const Seconds period = playbackDuration();
      if (period > 0.0F && elapsed_ >= period) {
        elapsed_ = std::fmod(elapsed_, period);
      }
    }
  }

  void seek(float progress) noexcept {
    if (!valid()) return;
    const float clamped = std::max(0.0F, std::min(1.0F, progress));
    elapsed_ = traversalDuration() * clamped;
    completed_ = false;
  }

  float progress() const noexcept {
    if (!valid() || frameCount_ <= 1) return 0.0F;
    const std::size_t local = localFrameIndex();
    return static_cast<float>(local) /
           static_cast<float>(frameCount_ - 1);
  }

  std::size_t frameIndex() const noexcept {
    return valid() ? firstFrame_ + localFrameIndex() : firstFrame_;
  }

  const SpriteFrame* frame() const noexcept {
    return valid() ? frameAt_(atlas_, frameIndex()) : nullptr;
  }

  bool completed() const noexcept { return completed_; }
  FramePlayback playback() const noexcept { return playback_; }
  Seconds frameDuration() const noexcept { return frameDuration_; }
  std::size_t frameCount() const noexcept { return frameCount_; }

 private:
  using FrameAt = const SpriteFrame* (*)(const void*, std::size_t) noexcept;

  template <std::size_t Capacity>
  static const SpriteFrame* atlasFrame(const void* atlas,
                                       std::size_t index) noexcept {
    return static_cast<const SpriteAtlas<Capacity>*>(atlas)->frame(index);
  }

  Seconds traversalDuration() const noexcept {
    return frameCount_ <= 1
               ? 0.0F
               : frameDuration_ * static_cast<float>(frameCount_ - 1);
  }

  Seconds onceDuration() const noexcept {
    return frameCount_ <= 1 ? frameDuration_ : traversalDuration();
  }

  Seconds playbackDuration() const noexcept {
    if (frameCount_ <= 1) return frameDuration_;
    const std::size_t steps = playback_ == FramePlayback::PingPong
                                  ? 2 * (frameCount_ - 1)
                                  : frameCount_;
    return frameDuration_ * static_cast<float>(steps);
  }

  std::size_t stepIndex() const noexcept {
    if (frameDuration_ <= 0.0F) return 0;
    const double biased = static_cast<double>(elapsed_) /
                              static_cast<double>(frameDuration_) +
                          1.0e-4;
    return static_cast<std::size_t>(std::floor(biased));
  }

  std::size_t localFrameIndex() const noexcept {
    if (frameCount_ <= 1) return 0;
    const std::size_t step = stepIndex();
    if (playback_ == FramePlayback::Once) {
      return std::min(step, frameCount_ - 1);
    }
    if (playback_ == FramePlayback::Loop) return step % frameCount_;
    const std::size_t period = 2 * (frameCount_ - 1);
    const std::size_t phase = step % period;
    return phase < frameCount_ ? phase : period - phase;
  }

  const void* atlas_ = nullptr;
  FrameAt frameAt_ = nullptr;
  std::size_t atlasSize_ = 0;
  std::size_t firstFrame_ = 0;
  std::size_t frameCount_ = 0;
  Seconds frameDuration_ = 0.0F;
  Seconds elapsed_ = 0.0F;
  FramePlayback playback_ = FramePlayback::Loop;
  bool completed_ = false;
};

}  // namespace cadenza
