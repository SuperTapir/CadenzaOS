#pragma once

#include <TFT_eSPI.h>

#include "display_profile.h"
#include "mono_canvas.h"

// T-Embed display adapter. A Sharp adapter can implement MonoCanvas without
// including TFT_eSPI or exposing its SPI/VCOM details to applications.
class TftMonoCanvas final : public MonoCanvas {
 public:
  explicit TftMonoCanvas(TFT_eSPI& display) : sprite_(&display) {}
  bool begin(const DisplayProfile& profile);
  void present();

  int16_t width() const override { return width_; }
  int16_t height() const override { return height_; }
  void clear(bool black = false) override;
  void pixel(int16_t x, int16_t y, bool black = true) override;
  void line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool black = true) override;
  void rect(int16_t x, int16_t y, int16_t w, int16_t h, bool black = true) override;
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool black = true) override;
  void circle(int16_t x, int16_t y, int16_t r, bool black = true) override;
  void fillCircle(int16_t x, int16_t y, int16_t r, bool black = true) override;
  void text(const char* value, int16_t x, int16_t y, uint8_t font = 2,
            bool black = true, TextAlign align = TextAlign::TopLeft) override;

 private:
  static uint16_t color(bool black) { return black ? TFT_BLACK : TFT_WHITE; }
  TFT_eSprite sprite_;
  int16_t width_ = 0;
  int16_t height_ = 0;
};
