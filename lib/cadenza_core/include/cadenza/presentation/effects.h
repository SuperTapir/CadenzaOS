#pragma once

#include <cstdint>

#include "cadenza/animation/spring.h"
#include "cadenza/core/core_types.h"

namespace cadenza {

class SelectionFeedback {
 public:
  explicit SelectionFeedback(MotionProfile profile) noexcept;
  void trigger() noexcept;
  void update(Seconds delta) noexcept;
  float scale() const noexcept { return spring_.value(); }
  bool active() const noexcept { return active_; }

 private:
  Spring spring_;
  float amplitude_ = 0.10F;
  Seconds elapsed_ = 0.0F;
  bool active_ = false;
};

class CameraPunch {
 public:
  CameraPunch(float amplitude, Seconds duration,
              MotionProfile profile = MotionProfile::Normal) noexcept;
  void trigger() noexcept;
  void update(Seconds delta) noexcept;
  float offset() const noexcept { return offset_; }
  bool active() const noexcept { return active_; }

 private:
  float amplitude_ = 0.0F;
  Seconds duration_ = 0.0F;
  Seconds elapsed_ = 0.0F;
  float offset_ = 0.0F;
  bool active_ = false;
};

class CameraShake {
 public:
  CameraShake(std::uint32_t seed, float amplitude, Seconds duration,
              MotionProfile profile = MotionProfile::Normal) noexcept;
  void trigger() noexcept;
  void update(Seconds delta) noexcept;
  float x() const noexcept { return x_; }
  float y() const noexcept { return y_; }
  float currentBound() const noexcept { return currentBound_; }
  bool active() const noexcept { return active_; }

 private:
  std::uint32_t nextRandom() noexcept;
  float randomSigned() noexcept;

  std::uint32_t initialSeed_ = 1;
  std::uint32_t state_ = 1;
  float amplitude_ = 0.0F;
  Seconds duration_ = 0.0F;
  Seconds elapsed_ = 0.0F;
  float x_ = 0.0F;
  float y_ = 0.0F;
  float currentBound_ = 0.0F;
  bool active_ = false;
};

}  // namespace cadenza
