#include "cadenza/animation/easing.h"

#include <algorithm>
#include <cmath>

namespace cadenza {
namespace {
float outBounce(float progress) noexcept {
  constexpr float n1 = 7.5625F;
  constexpr float d1 = 2.75F;
  if (progress < 1.0F / d1) return n1 * progress * progress;
  if (progress < 2.0F / d1) {
    progress -= 1.5F / d1;
    return n1 * progress * progress + 0.75F;
  }
  if (progress < 2.5F / d1) {
    progress -= 2.25F / d1;
    return n1 * progress * progress + 0.9375F;
  }
  progress -= 2.625F / d1;
  return n1 * progress * progress + 0.984375F;
}
}  // namespace

float ease(Easing easing, float progress) noexcept {
  const float t = std::max(0.0F, std::min(1.0F, progress));
  if (t <= 0.0F) return 0.0F;
  if (t >= 1.0F) return 1.0F;
  switch (easing) {
    case Easing::Linear:
      return t;
    case Easing::InQuad:
      return t * t;
    case Easing::OutQuad:
      return 1.0F - (1.0F - t) * (1.0F - t);
    case Easing::InOutQuad:
      return t < 0.5F ? 2.0F * t * t
                      : 1.0F - std::pow(-2.0F * t + 2.0F, 2.0F) / 2.0F;
    case Easing::InOutCubic:
      return t < 0.5F ? 4.0F * t * t * t
                      : 1.0F - std::pow(-2.0F * t + 2.0F, 3.0F) / 2.0F;
    case Easing::OutExpo:
      return 1.0F - std::pow(2.0F, -10.0F * t);
    case Easing::OutBack: {
      constexpr float c1 = 1.70158F;
      constexpr float c3 = c1 + 1.0F;
      const float shifted = t - 1.0F;
      return 1.0F + c3 * shifted * shifted * shifted +
             c1 * shifted * shifted;
    }
    case Easing::OutBounce:
      return outBounce(t);
    case Easing::OutElastic: {
      constexpr float pi = 3.14159265358979323846F;
      constexpr float c4 = 2.0F * pi / 3.0F;
      return std::pow(2.0F, -10.0F * t) *
                 std::sin((t * 10.0F - 0.75F) * c4) +
             1.0F;
    }
  }
  return t;
}

}  // namespace cadenza
