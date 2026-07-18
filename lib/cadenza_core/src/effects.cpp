#include "cadenza/presentation/effects.h"

#include <algorithm>
#include <cmath>

#include "cadenza/presentation/defaults.h"

namespace cadenza {
namespace {
float profileScale(MotionProfile profile) noexcept {
  return profile == MotionProfile::Reduced
             ? presentation_defaults::kReducedMotionScale
             : 1.0F;
}
}  // namespace

SelectionFeedback::SelectionFeedback(MotionProfile profile) noexcept
    : spring_(1.0F, {240.0F, 22.0F, 1.0F}),
      amplitude_(presentation_defaults::kSelectionOvershoot *
                 profileScale(profile)) {}

void SelectionFeedback::trigger() noexcept {
  elapsed_ = 0.0F;
  active_ = true;
  spring_.setTarget(1.0F + amplitude_);
}

void SelectionFeedback::update(Seconds delta) noexcept {
  if (!active_) return;
  elapsed_ += std::max(0.0F, delta);
  if (elapsed_ >= 0.05F) spring_.setTarget(1.0F);
  spring_.update(delta);
  if (elapsed_ >= 0.05F && spring_.settled()) active_ = false;
}

CameraPunch::CameraPunch(float amplitude, Seconds duration,
                         MotionProfile profile) noexcept
    : amplitude_(std::abs(amplitude) * profileScale(profile)),
      duration_(std::max(0.0F, duration)) {}

void CameraPunch::trigger() noexcept {
  elapsed_ = 0.0F;
  offset_ = 0.0F;
  active_ = duration_ > 0.0F;
}

void CameraPunch::update(Seconds delta) noexcept {
  if (!active_) return;
  elapsed_ += std::max(0.0F, delta);
  if (elapsed_ >= duration_) {
    elapsed_ = duration_;
    offset_ = 0.0F;
    active_ = false;
    return;
  }
  constexpr float pi = 3.14159265358979323846F;
  const float progress = elapsed_ / duration_;
  offset_ = amplitude_ * std::sin(progress * pi) * (1.0F - progress);
}

CameraShake::CameraShake(std::uint32_t seed, float amplitude, Seconds duration,
                         MotionProfile profile) noexcept
    : initialSeed_(seed == 0 ? 1U : seed),
      state_(initialSeed_),
      amplitude_(std::abs(amplitude) * profileScale(profile)),
      duration_(std::max(0.0F, duration)) {}

void CameraShake::trigger() noexcept {
  state_ = initialSeed_;
  elapsed_ = 0.0F;
  x_ = 0.0F;
  y_ = 0.0F;
  currentBound_ = amplitude_;
  active_ = duration_ > 0.0F;
}

void CameraShake::update(Seconds delta) noexcept {
  if (!active_) return;
  elapsed_ += std::max(0.0F, delta);
  if (elapsed_ >= duration_) {
    elapsed_ = duration_;
    x_ = 0.0F;
    y_ = 0.0F;
    currentBound_ = 0.0F;
    active_ = false;
    return;
  }
  const float remaining = 1.0F - elapsed_ / duration_;
  currentBound_ = amplitude_ * remaining * remaining;
  x_ = randomSigned() * currentBound_;
  y_ = randomSigned() * currentBound_;
}

std::uint32_t CameraShake::nextRandom() noexcept {
  state_ ^= state_ << 13U;
  state_ ^= state_ >> 17U;
  state_ ^= state_ << 5U;
  return state_;
}

float CameraShake::randomSigned() noexcept {
  return static_cast<float>(nextRandom() & 0x00FFFFFFU) / 8388607.5F - 1.0F;
}

}  // namespace cadenza
