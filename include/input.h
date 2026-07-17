#pragma once

#include <Arduino.h>

struct InputFrame {
  int8_t turn = 0;
  bool pressed = false;
  bool released = false;
  bool clicked = false;
  bool longPressed = false;
  uint32_t heldMs = 0;
};

class InputController {
 public:
  void begin();
  // Sampling is intentionally independent from rendering so encoder edges are
  // not lost when a frame is expensive to draw or transfer.
  void sample();
  InputFrame takeFrame();

 private:
  int lastEncoderA_ = HIGH;
  bool stableButton_ = HIGH;
  bool sampledButton_ = HIGH;
  bool longPressSent_ = false;
  uint32_t sampleChangedAt_ = 0;
  uint32_t pressedAt_ = 0;
  InputFrame pending_;
};
