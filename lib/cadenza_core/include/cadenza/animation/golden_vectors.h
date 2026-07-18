#pragma once

#include <array>

#include "cadenza/animation/easing.h"

namespace cadenza {

struct EasingGoldenVector {
  Easing easing;
  float progress;
  float expected;
};

inline constexpr std::array<EasingGoldenVector, 12> kEasingGoldenVectors{{
    {Easing::Linear, 0.25F, 0.25F},
    {Easing::InQuad, 0.25F, 0.0625F},
    {Easing::OutQuad, 0.25F, 0.4375F},
    {Easing::InOutQuad, 0.25F, 0.125F},
    {Easing::InOutQuad, 0.75F, 0.875F},
    {Easing::InOutCubic, 0.25F, 0.0625F},
    {Easing::InOutCubic, 0.75F, 0.9375F},
    {Easing::OutExpo, 0.5F, 0.96875F},
    {Easing::OutBack, 0.5F, 1.0876975F},
    {Easing::OutBounce, 0.5F, 0.765625F},
    {Easing::OutElastic, 0.5F, 1.015625F},
    {Easing::OutBounce, 0.75F, 0.97265625F},
}};

}  // namespace cadenza
