#pragma once

#include <cstdint>

#include "cadenza/core/mono_canvas.h"

namespace cadenza::presentation {

struct TimerNumeralMetrics {
  std::int32_t digitWidth = 0;
  std::int32_t colonWidth = 0;
  std::int32_t height = 0;

  constexpr std::int32_t displayWidth() const noexcept {
    return digitWidth * 4 + colonWidth;
  }
};

TimerNumeralMetrics timerNumeralMetrics(std::int32_t canvasWidth) noexcept;
Rect renderTimerNumerals(MonoCanvas& canvas, std::uint32_t displayMs,
                         std::int32_t top) noexcept;

}  // namespace cadenza::presentation
