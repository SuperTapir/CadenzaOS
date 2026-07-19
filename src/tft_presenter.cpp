#include "tft_presenter.h"

#include <array>
#include <cstdint>

bool TftPresenter::present(
    const cadenza::MonoFramebuffer& framebuffer) noexcept {
  if (!framebuffer.valid()) return false;

  const std::int32_t width = framebuffer.width();
  const std::int32_t height = framebuffer.height();
  if (width <= 0 || height <= 0 || width > 320) {
    return false;
  }

  // Convert 1-bit MSB rows to RGB565 and push as whole lines. TFT_eSPI's
  // bpp=false pushImage path walks every pixel through a slow generic loop and
  // was measured at ~30 ms/frame on T-Embed; bulk pushColors is much cheaper.
  // Unroll 8 pixels per source byte to cut convert overhead before SPI.
  std::array<std::uint16_t, 320> line{};
  const std::uint8_t* data = framebuffer.data();
  const std::size_t stride = framebuffer.stride();

  display_.startWrite();
  display_.setAddrWindow(0, 0, width, height);
  for (std::int32_t y = 0; y < height; ++y) {
    const std::uint8_t* row = data + static_cast<std::size_t>(y) * stride;
    std::int32_t x = 0;
    while (x + 8 <= width) {
      const std::uint8_t cell = row[static_cast<std::size_t>(x) >> 3];
      line[static_cast<std::size_t>(x) + 0] =
          (cell & 0x80U) != 0 ? TFT_BLACK : TFT_WHITE;
      line[static_cast<std::size_t>(x) + 1] =
          (cell & 0x40U) != 0 ? TFT_BLACK : TFT_WHITE;
      line[static_cast<std::size_t>(x) + 2] =
          (cell & 0x20U) != 0 ? TFT_BLACK : TFT_WHITE;
      line[static_cast<std::size_t>(x) + 3] =
          (cell & 0x10U) != 0 ? TFT_BLACK : TFT_WHITE;
      line[static_cast<std::size_t>(x) + 4] =
          (cell & 0x08U) != 0 ? TFT_BLACK : TFT_WHITE;
      line[static_cast<std::size_t>(x) + 5] =
          (cell & 0x04U) != 0 ? TFT_BLACK : TFT_WHITE;
      line[static_cast<std::size_t>(x) + 6] =
          (cell & 0x02U) != 0 ? TFT_BLACK : TFT_WHITE;
      line[static_cast<std::size_t>(x) + 7] =
          (cell & 0x01U) != 0 ? TFT_BLACK : TFT_WHITE;
      x += 8;
    }
    for (; x < width; ++x) {
      const bool black =
          (row[static_cast<std::size_t>(x) >> 3] &
           static_cast<std::uint8_t>(0x80U >> (x & 7))) != 0;
      line[static_cast<std::size_t>(x)] = black ? TFT_BLACK : TFT_WHITE;
    }
    display_.pushColors(line.data(), static_cast<std::uint32_t>(width), true);
  }
  display_.endWrite();
  return true;
}
