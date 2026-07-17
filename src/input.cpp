#include "input.h"

#include "board_pins.h"

namespace {
constexpr uint32_t kDebounceMs = 22;
constexpr uint32_t kLongPressMs = 650;
}

void InputController::begin() {
  pinMode(BoardPins::kEncoderA, INPUT_PULLUP);
  pinMode(BoardPins::kEncoderB, INPUT_PULLUP);
  pinMode(BoardPins::kEncoderButton, INPUT_PULLUP);
  lastEncoderA_ = digitalRead(BoardPins::kEncoderA);
  stableButton_ = sampledButton_ = digitalRead(BoardPins::kEncoderButton);
}

void InputController::sample() {
  const uint32_t now = millis();

  const int encoderA = digitalRead(BoardPins::kEncoderA);
  if (encoderA != lastEncoderA_ && encoderA == LOW) {
    pending_.turn += digitalRead(BoardPins::kEncoderB) != encoderA ? 1 : -1;
  }
  lastEncoderA_ = encoderA;

  const bool sample = digitalRead(BoardPins::kEncoderButton);
  if (sample != sampledButton_) {
    sampledButton_ = sample;
    sampleChangedAt_ = now;
  }
  if (sample != stableButton_ && now - sampleChangedAt_ >= kDebounceMs) {
    stableButton_ = sample;
    if (stableButton_ == LOW) {
      pending_.pressed = true;
      pressedAt_ = now;
      longPressSent_ = false;
    } else {
      pending_.released = true;
      pending_.clicked = !longPressSent_;
    }
  }

  if (stableButton_ == LOW) {
    pending_.heldMs = now - pressedAt_;
    if (!longPressSent_ && pending_.heldMs >= kLongPressMs) {
      longPressSent_ = true;
      pending_.longPressed = true;
    }
  }
}

InputFrame InputController::takeFrame() {
  InputFrame frame = pending_;
  pending_.turn = 0;
  pending_.pressed = pending_.released = pending_.clicked = pending_.longPressed = false;
  if (stableButton_ != LOW) pending_.heldMs = 0;
  return frame;
}
