#include "tft_mono_canvas.h"

namespace {
uint8_t tftDatum(TextAlign align) {
  switch (align) {
    case TextAlign::MiddleLeft: return ML_DATUM;
    case TextAlign::MiddleCenter: return MC_DATUM;
    case TextAlign::MiddleRight: return MR_DATUM;
    case TextAlign::BottomLeft: return BL_DATUM;
    case TextAlign::BottomRight: return BR_DATUM;
    case TextAlign::TopLeft:
    default: return TL_DATUM;
  }
}
}  // namespace

bool TftMonoCanvas::begin(const DisplayProfile& profile) {
  width_ = profile.width;
  height_ = profile.height;
  sprite_.setColorDepth(1);
  sprite_.setBitmapColor(TFT_BLACK, TFT_WHITE);
  sprite_.setTextWrap(false);
  return sprite_.createSprite(profile.width, profile.height) != nullptr;
}

void TftMonoCanvas::present() { sprite_.pushSprite(0, 0); }
void TftMonoCanvas::clear(bool black) { sprite_.fillSprite(color(black)); }
void TftMonoCanvas::pixel(int16_t x, int16_t y, bool black) { sprite_.drawPixel(x, y, color(black)); }
void TftMonoCanvas::line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, bool black) {
  sprite_.drawLine(x0, y0, x1, y1, color(black));
}
void TftMonoCanvas::rect(int16_t x, int16_t y, int16_t w, int16_t h, bool black) {
  sprite_.drawRect(x, y, w, h, color(black));
}
void TftMonoCanvas::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, bool black) {
  sprite_.fillRect(x, y, w, h, color(black));
}
void TftMonoCanvas::circle(int16_t x, int16_t y, int16_t r, bool black) {
  sprite_.drawCircle(x, y, r, color(black));
}
void TftMonoCanvas::fillCircle(int16_t x, int16_t y, int16_t r, bool black) {
  sprite_.fillCircle(x, y, r, color(black));
}
void TftMonoCanvas::text(const char* value, int16_t x, int16_t y, uint8_t font,
                         bool black, TextAlign align) {
  sprite_.setTextDatum(tftDatum(align));
  sprite_.setTextColor(color(black));
  sprite_.drawString(value, x, y, font);
}
