#include "cadenza/presentation/timer_numerals.h"

#include "cadenza/presentation/generated/timer_digits_sharp.h"
#include "cadenza/presentation/generated/timer_digits_t_embed.h"

namespace cadenza::presentation {
namespace {

const BitmapView& timerNumeralBitmap(std::int32_t canvasWidth) noexcept {
  return canvasWidth >= 400 ? kTimerDigitsSharp : kTimerDigitsTEmbed;
}

}  // namespace

TimerNumeralMetrics timerNumeralMetrics(std::int32_t canvasWidth) noexcept {
  return canvasWidth >= 400 ? TimerNumeralMetrics{68, 36, 120}
                            : TimerNumeralMetrics{50, 28, 84};
}

Rect renderTimerNumerals(MonoCanvas& canvas, std::uint32_t displayMs,
                         std::int32_t top) noexcept {
  const auto metrics = timerNumeralMetrics(canvas.width());
  const auto& bitmap = timerNumeralBitmap(canvas.width());
  const std::uint32_t seconds = (displayMs + 999U) / 1000U;
  const unsigned values[] = {
      static_cast<unsigned>((seconds / 600U) % 10U),
      static_cast<unsigned>((seconds / 60U) % 10U),
      10U,
      static_cast<unsigned>((seconds / 10U) % 6U),
      static_cast<unsigned>(seconds % 10U),
  };
  int destinationX = (canvas.width() - metrics.displayWidth()) / 2;
  const int left = destinationX;
  for (const unsigned glyph : values) {
    const bool colon = glyph == 10U;
    const int glyphWidth = colon ? metrics.colonWidth : metrics.digitWidth;
    const int sourceX = colon ? metrics.digitWidth * 10
                              : static_cast<int>(glyph) * metrics.digitWidth;
    canvas.drawBitmap(bitmap, {sourceX, 0, glyphWidth, metrics.height},
                      destinationX, top,
                      {BitmapComposition::SetBlack, false, false});
    destinationX += glyphWidth;
  }
  return {left, top, metrics.displayWidth(), metrics.height};
}

}  // namespace cadenza::presentation
