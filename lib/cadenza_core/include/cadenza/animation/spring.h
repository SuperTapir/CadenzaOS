#pragma once

#include <cstdint>

#include "cadenza/core/core_types.h"

namespace cadenza {

struct SpringConfig {
  float stiffness = 180.0F;
  float damping = 18.0F;
  float mass = 1.0F;
  Seconds fixedStep = 1.0F / 120.0F;
  std::uint16_t maxSubsteps = 32;
  float positionEpsilon = 0.0005F;
  float velocityEpsilon = 0.0005F;
};

class Spring {
 public:
  explicit Spring(float initial = 0.0F,
                  SpringConfig config = {}) noexcept;

  void setTarget(float target) noexcept { target_ = target; }
  void update(Seconds delta) noexcept;
  void reset(float value) noexcept;

  float value() const noexcept { return value_; }
  float target() const noexcept { return target_; }
  float velocity() const noexcept { return velocity_; }
  bool settled() const noexcept { return settled_; }
  std::uint16_t lastSubsteps() const noexcept { return lastSubsteps_; }

 private:
  SpringConfig config_;
  float value_ = 0.0F;
  float target_ = 0.0F;
  float velocity_ = 0.0F;
  std::uint16_t lastSubsteps_ = 0;
  bool settled_ = true;
};

}  // namespace cadenza
