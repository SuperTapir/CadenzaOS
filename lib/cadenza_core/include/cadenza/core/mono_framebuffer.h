#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace cadenza {

enum class FramebufferProfile : std::uint8_t {
  TEmbed,
  Sharp,
};

class MonoFramebuffer {
 public:
  static constexpr std::size_t kCapacityBytes = 400U * 240U / 8U;

  explicit MonoFramebuffer(FramebufferProfile profile) noexcept;

  bool valid() const noexcept { return width_ > 0 && height_ > 0; }
  std::int16_t width() const noexcept { return width_; }
  std::int16_t height() const noexcept { return height_; }
  std::size_t stride() const noexcept { return stride_; }
  std::size_t sizeBytes() const noexcept { return sizeBytes_; }
  constexpr std::size_t capacityBytes() const noexcept { return kCapacityBytes; }

  std::uint8_t* data() noexcept { return storage_.data(); }
  const std::uint8_t* data() const noexcept { return storage_.data(); }

  void clear(bool black = false) noexcept;
  bool setPixel(std::int32_t x, std::int32_t y, bool black = true) noexcept;
  bool pixel(std::int32_t x, std::int32_t y) const noexcept;

 private:
  bool contains(std::int32_t x, std::int32_t y) const noexcept;

  std::array<std::uint8_t, kCapacityBytes> storage_{};
  std::int16_t width_ = 0;
  std::int16_t height_ = 0;
  std::size_t stride_ = 0;
  std::size_t sizeBytes_ = 0;
};

static_assert(MonoFramebuffer::kCapacityBytes == 12000,
              "400x240 packed framebuffer must remain 12,000 bytes");

}  // namespace cadenza
