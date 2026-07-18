#include "tft_presenter.h"

bool TftPresenter::present(
    const cadenza::MonoFramebuffer& framebuffer) noexcept {
  if (!framebuffer.valid()) return false;
  display_.setBitmapColor(TFT_BLACK, TFT_WHITE);
  display_.pushImage(0, 0, framebuffer.width(), framebuffer.height(),
                     framebuffer.data(), false, nullptr);
  return true;
}
