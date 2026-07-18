#pragma once

#include <cstdint>

namespace cadenza {

enum class Easing : std::uint8_t {
  Linear,
  InQuad,
  OutQuad,
  InOutQuad,
  InOutCubic,
  OutExpo,
  OutBack,
  OutBounce,
  OutElastic,
};

float ease(Easing easing, float progress) noexcept;

}  // namespace cadenza
