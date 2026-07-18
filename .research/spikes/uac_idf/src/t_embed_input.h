#pragma once

#include <array>
#include <cstddef>

#include "cadenza/core/input.h"
#include "esp_err.h"

namespace cadenza::idf {

class TEmbedInput {
 public:
  esp_err_t start() noexcept;
  void sample() noexcept;
  InputFrame takeFrame() noexcept;

 private:
  static constexpr std::size_t kCapacity = 16;
  bool enqueue(RawInputType type, std::int32_t value,
               MonotonicMillis timestamp) noexcept;
  static MonotonicMillis nowMs() noexcept;

  std::array<RawInputEvent, kCapacity> queue_{};
  std::size_t read_ = 0;
  std::size_t write_ = 0;
  int lastEncoderA_ = 1;
  bool lastButtonDown_ = false;
  InputReducer reducer_{};
};

}  // namespace cadenza::idf
