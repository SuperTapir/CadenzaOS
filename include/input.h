#pragma once

#include <Arduino.h>

#include "cadenza/core/input.h"
#include "cadenza/core/input_adapter.h"

using InputFrame = cadenza::InputFrame;

class TEmbedRawInputSource final : public cadenza::RawInputSource,
                                   public cadenza::MonotonicClock {
 public:
  void begin();
  void sample();
  bool poll(cadenza::RawInputEvent& event) noexcept override;
  cadenza::MonotonicMillis nowMs() const noexcept override;

 private:
  static constexpr uint8_t kQueueCapacity = 16;
  bool enqueue(cadenza::RawInputType type, int32_t value,
               cadenza::MonotonicMillis timestampMs) noexcept;

  cadenza::RawInputEvent queue_[kQueueCapacity] = {};
  uint8_t readPosition_ = 0;
  uint8_t writePosition_ = 0;
  int lastEncoderA_ = HIGH;
  bool lastButtonDown_ = false;
};

class InputController {
 public:
  void begin();
  // Sampling is intentionally independent from rendering so encoder edges are
  // not lost when a frame is expensive to draw or transfer.
  void sample();
 InputFrame takeFrame();

 private:
  TEmbedRawInputSource source_;
  cadenza::InputReducer reducer_;
};
