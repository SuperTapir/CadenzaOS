#include "cadenza/core/mono_framebuffer.h"

#include <algorithm>

namespace cadenza {

MonoFramebuffer::MonoFramebuffer(FramebufferProfile profile) noexcept {
  switch (profile) {
    case FramebufferProfile::TEmbed:
      width_ = 320;
      height_ = 170;
      break;
    case FramebufferProfile::Sharp:
      width_ = 400;
      height_ = 240;
      break;
  }
  stride_ = (static_cast<std::size_t>(width_) + 7U) / 8U;
  sizeBytes_ = stride_ * static_cast<std::size_t>(height_);
}

void MonoFramebuffer::clear(bool black) noexcept {
  std::fill_n(storage_.begin(), sizeBytes_, black ? 0xFFU : 0x00U);
}

bool MonoFramebuffer::setPixel(std::int32_t x, std::int32_t y,
                               bool black) noexcept {
  if (!contains(x, y)) {
    return false;
  }
  const std::size_t offset = static_cast<std::size_t>(y) * stride_ +
                             static_cast<std::size_t>(x) / 8U;
  const auto mask = static_cast<std::uint8_t>(0x80U >> (x & 7));
  if (black) {
    storage_[offset] |= mask;
  } else {
    storage_[offset] &= static_cast<std::uint8_t>(~mask);
  }
  return true;
}

bool MonoFramebuffer::pixel(std::int32_t x, std::int32_t y) const noexcept {
  if (!contains(x, y)) {
    return false;
  }
  const std::size_t offset = static_cast<std::size_t>(y) * stride_ +
                             static_cast<std::size_t>(x) / 8U;
  const auto mask = static_cast<std::uint8_t>(0x80U >> (x & 7));
  return (storage_[offset] & mask) != 0;
}

bool MonoFramebuffer::contains(std::int32_t x, std::int32_t y) const noexcept {
  return x >= 0 && y >= 0 && x < width_ && y < height_;
}

}  // namespace cadenza
