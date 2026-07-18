#include "cadenza/animation/spring.h"

#include <algorithm>
#include <cmath>

namespace cadenza {

Spring::Spring(float initial, SpringConfig config) noexcept
    : config_(config), value_(initial), target_(initial) {
  if (config_.stiffness < 0.0F) config_.stiffness = 0.0F;
  if (config_.damping < 0.0F) config_.damping = 0.0F;
  if (config_.mass <= 0.0F) config_.mass = 1.0F;
  if (config_.fixedStep <= 0.0F) config_.fixedStep = 1.0F / 120.0F;
  if (config_.maxSubsteps == 0) config_.maxSubsteps = 1;
  if (config_.positionEpsilon < 0.0F) config_.positionEpsilon = 0.0F;
  if (config_.velocityEpsilon < 0.0F) config_.velocityEpsilon = 0.0F;
}

void Spring::update(Seconds delta) noexcept {
  lastSubsteps_ = 0;
  Seconds remaining = std::min(
      std::max(0.0F, delta),
      config_.fixedStep * static_cast<float>(config_.maxSubsteps));
  while (remaining > 0.0F && lastSubsteps_ < config_.maxSubsteps) {
    const Seconds step = std::min(config_.fixedStep, remaining);
    const float force = -config_.stiffness * (value_ - target_) -
                        config_.damping * velocity_;
    const float acceleration = force / config_.mass;
    velocity_ += acceleration * step;
    value_ += velocity_ * step;
    remaining -= step;
    ++lastSubsteps_;
  }

  settled_ = std::abs(value_ - target_) <= config_.positionEpsilon &&
             std::abs(velocity_) <= config_.velocityEpsilon;
  if (settled_) {
    value_ = target_;
    velocity_ = 0.0F;
  }
}

void Spring::reset(float value) noexcept {
  value_ = value;
  target_ = value;
  velocity_ = 0.0F;
  lastSubsteps_ = 0;
  settled_ = true;
}

}  // namespace cadenza
