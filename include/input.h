#pragma once

#include <Arduino.h>

#include <cstdint>

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
  static constexpr uint8_t kQueueCapacity = 32;
  // T-Embed factory firmware uses RotaryEncoder LatchMode::TWO03 (two
  // counts per mechanical detent). Full-quad PCNT matches that 2:1 ratio;
  // folding by 4 made the wheel feel half-sensitive / not "following" the hand.
  static constexpr std::int32_t kStepsPerDetent = 2;

  bool enqueue(cadenza::RawInputType type, int32_t value,
               cadenza::MonotonicMillis timestampMs) noexcept;
  void drainEncoder(cadenza::MonotonicMillis timestampMs) noexcept;

  cadenza::RawInputEvent queue_[kQueueCapacity] = {};
  uint8_t readPosition_ = 0;
  uint8_t writePosition_ = 0;
  std::int32_t encoderStepResidue_ = 0;
  std::int32_t lastEncoderCount_ = 0;
  bool encoderReady_ = false;
  bool lastButtonDown_ = false;
};

class InputController {
 public:
  void begin();
  // Sampling drains the PCNT hardware counter into logical turns. Call this
  // both while waiting for the next frame and after heavy present work so UI
  // never starves input even when rendering is slow.
  void sample();
  InputFrame takeFrame();

 private:
  TEmbedRawInputSource source_;
  cadenza::InputReducer reducer_;
};
