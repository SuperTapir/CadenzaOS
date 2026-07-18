#include "input.h"

#include "board_pins.h"

void TEmbedRawInputSource::begin() {
  pinMode(BoardPins::kEncoderA, INPUT_PULLUP);
  pinMode(BoardPins::kEncoderB, INPUT_PULLUP);
  pinMode(BoardPins::kEncoderButton, INPUT_PULLUP);
  lastEncoderA_ = digitalRead(BoardPins::kEncoderA);
  lastButtonDown_ = digitalRead(BoardPins::kEncoderButton) == LOW;
  if (lastButtonDown_) {
    enqueue(cadenza::RawInputType::ButtonDown, 0, nowMs());
  }
}

void TEmbedRawInputSource::sample() {
  const auto now = nowMs();

  const int encoderA = digitalRead(BoardPins::kEncoderA);
  if (encoderA != lastEncoderA_ && encoderA == LOW) {
    enqueue(cadenza::RawInputType::Turn,
            digitalRead(BoardPins::kEncoderB) != encoderA ? 1 : -1, now);
  }
  lastEncoderA_ = encoderA;

  const bool buttonDown = digitalRead(BoardPins::kEncoderButton) == LOW;
  if (buttonDown != lastButtonDown_) {
    lastButtonDown_ = buttonDown;
    enqueue(buttonDown ? cadenza::RawInputType::ButtonDown
                       : cadenza::RawInputType::ButtonUp,
            0, now);
  }
}

bool TEmbedRawInputSource::poll(cadenza::RawInputEvent& event) noexcept {
  if (readPosition_ == writePosition_) {
    return false;
  }
  event = queue_[readPosition_];
  readPosition_ = (readPosition_ + 1) % kQueueCapacity;
  return true;
}

cadenza::MonotonicMillis TEmbedRawInputSource::nowMs() const noexcept {
  return millis();
}

bool TEmbedRawInputSource::enqueue(cadenza::RawInputType type, int32_t value,
                                   cadenza::MonotonicMillis timestampMs) noexcept {
  const uint8_t next = (writePosition_ + 1) % kQueueCapacity;
  if (next == readPosition_) {
    return false;
  }
  queue_[writePosition_] = {type, timestampMs, value};
  writePosition_ = next;
  return true;
}

void InputController::begin() { source_.begin(); }

void InputController::sample() {
  source_.sample();
  cadenza::pumpInput(source_, source_, reducer_);
}

InputFrame InputController::takeFrame() {
  reducer_.advanceTo(source_.nowMs());
  return reducer_.takeFrame();
}
