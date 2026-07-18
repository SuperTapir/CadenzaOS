#pragma once

#include "cadenza/core/core_types.h"

namespace cadenza::presentation_defaults {

constexpr Seconds kAppTransitionDuration = 0.32F;
constexpr Seconds kGalleryCycleDuration = 2.0F;
constexpr float kSelectionOvershoot = 0.10F;
constexpr float kCameraPunchAmplitude = 10.0F;
constexpr Seconds kCameraPunchDuration = 0.35F;
constexpr float kCameraShakeAmplitude = 7.0F;
constexpr Seconds kCameraShakeDuration = 0.55F;
constexpr float kReducedMotionScale = 0.35F;
constexpr float kScrubStep = 0.05F;

}  // namespace cadenza::presentation_defaults
