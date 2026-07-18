#define STB_IMAGE_WRITE_IMPLEMENTATION
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif
#include "stb_image_write.h"
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include "cadenza/desktop/png_writer.h"

#include <cstdio>
#include <vector>

namespace cadenza::desktop {

std::string screenshotFilename(std::int16_t width, std::int16_t height,
                               FrameIndex frame,
                               std::uint64_t simulationMilliseconds) {
  char value[96];
  std::snprintf(value, sizeof(value),
                "cadenza-%dx%d-f%08llu-t%09llums.png", width, height,
                static_cast<unsigned long long>(frame),
                static_cast<unsigned long long>(simulationMilliseconds));
  return value;
}

bool writePng(const std::string& path,
              const MonoFramebuffer& framebuffer) noexcept {
  if (!framebuffer.valid() || path.empty()) return false;
  std::vector<std::uint8_t> grayscale(
      static_cast<std::size_t>(framebuffer.width()) * framebuffer.height());
  for (std::int32_t y = 0; y < framebuffer.height(); ++y) {
    for (std::int32_t x = 0; x < framebuffer.width(); ++x) {
      grayscale[static_cast<std::size_t>(y) * framebuffer.width() + x] =
          framebuffer.pixel(x, y) ? 0 : 255;
    }
  }
  return stbi_write_png(path.c_str(), framebuffer.width(), framebuffer.height(),
                        1, grayscale.data(), framebuffer.width()) != 0;
}

}  // namespace cadenza::desktop
