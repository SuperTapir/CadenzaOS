#pragma once

#include <cstdint>

#include "cadenza/core/core_types.h"

namespace cadenza::presentation_defaults {

constexpr Seconds kAppTransitionDuration = 0.32F;
constexpr Seconds kAppLaunchHandoffDuration = 0.80F;
constexpr Seconds kAppReturnHandoffDuration = 0.44F;
constexpr Seconds kReducedAppLaunchHandoffDuration = 0.56F;
constexpr Seconds kReducedAppReturnHandoffDuration = 0.28F;
constexpr Seconds kGalleryCycleDuration = 2.0F;
constexpr float kSelectionOvershoot = 0.10F;
constexpr float kCameraPunchAmplitude = 10.0F;
constexpr Seconds kCameraPunchDuration = 0.35F;
constexpr float kCameraShakeAmplitude = 7.0F;
constexpr Seconds kCameraShakeDuration = 0.55F;
constexpr float kReducedMotionScale = 0.35F;
constexpr float kScrubStep = 0.05F;
constexpr Seconds kPassiveIndicatorMotionDuration = 0.18F;
constexpr Seconds kReducedPassiveIndicatorMotionDuration = 0.10F;
constexpr std::int32_t kPassiveIndicatorRestY = 2;
constexpr std::int32_t kPassiveIndicatorTravelY = 32;

}  // namespace cadenza::presentation_defaults
