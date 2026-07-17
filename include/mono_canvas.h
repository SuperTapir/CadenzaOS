#pragma once

#include <Arduino.h>

enum class TextAlign : uint8_t {
  TopLeft,
  MiddleLeft,
  MiddleCenter,
  MiddleRight,
  BottomLeft,
  BottomRight,
};

// Hardware-independent 1-bit drawing contract used by the runtime and apps.
class MonoCanvas {
 public:
  virtual ~MonoCanvas() = default;
  virtual int16_t width() const = 0;
  virtual int16_t height() const = 0;
  virtual void clear(bool black = false) = 0;
  virtual void pixel(int16_t x, int16_t y, bool black = true) = 0;
  virtual void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool black = true) = 0;
  virtual void rect(int16_t x, int16_t y, int16_t w, int16_t h, bool black = true) = 0;
  virtual void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool black = true) = 0;
  virtual void circle(int16_t x, int16_t y, int16_t r, bool black = true) = 0;
  virtual void fillCircle(int16_t x, int16_t y, int16_t r, bool black = true) = 0;
  virtual void text(const char* value, int16_t x, int16_t y, uint8_t font = 2,
                    bool black = true, TextAlign align = TextAlign::TopLeft) = 0;
};
