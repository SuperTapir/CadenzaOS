#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/input.h"
#include "esp_err.h"

namespace cadenza::idf {

// T-Embed encoder/button owner. Matches PlatformIO PCNT full-quadrature
// decode so slow present frames never drop detents.
class TEmbedInput {
 public:
  esp_err_t start() noexcept;
  // Drain PCNT + button edges into the reducer. Call while waiting for the
  // next frame and again after heavy present work.
  void sample() noexcept;
  InputFrame takeFrame() noexcept;

 private:
  static constexpr std::size_t kCapacity = 32;
  // Same TWO03 detent ratio as PlatformIO / T-Embed factory firmware.
  static constexpr std::int32_t kStepsPerDetent = 2;

  bool enqueue(RawInputType type, std::int32_t value,
               MonotonicMillis timestamp) noexcept;
  void drainEncoder(MonotonicMillis timestamp) noexcept;
  static MonotonicMillis nowMs() noexcept;

  std::array<RawInputEvent, kCapacity> queue_{};
  std::size_t read_ = 0;
  std::size_t write_ = 0;
  std::int32_t encoderStepResidue_ = 0;
  std::int32_t lastEncoderCount_ = 0;
  bool encoderReady_ = false;
  bool lastButtonDown_ = false;
  InputReducer reducer_{};
};

}  // namespace cadenza::idf
