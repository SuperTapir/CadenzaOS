#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/diagnostics.h"
#include "cadenza/core/mono_framebuffer.h"

namespace cadenza {

struct Rect {
  std::int32_t x = 0;
  std::int32_t y = 0;
  std::int32_t width = 0;
  std::int32_t height = 0;
};

enum class TextAlign : std::uint8_t {
  TopLeft,
  MiddleLeft,
  MiddleCenter,
  MiddleRight,
  BottomLeft,
  BottomRight,
};

struct TextMetrics {
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::int32_t ascent = 0;
  std::int32_t descent = 0;
};

enum class TextOverflowPolicy : std::uint8_t {
  Ellipsis,
  Wrap,
  Marquee,
};

enum class BoundedTextStatus : std::uint8_t {
  Invalid,
  NoFit,
  Complete,
  Scaled,
  Truncated,
  Wrapped,
  Marquee,
};

constexpr std::size_t kBoundedTextMaxLines = 4;
constexpr std::size_t kBoundedTextLineCapacity = 96;

struct BoundedTextRequest {
  const char* value = nullptr;
  Rect bounds;
  std::uint8_t preferredScale = 1;
  std::uint8_t minimumScale = 1;
  TextAlign align = TextAlign::TopLeft;
  TextOverflowPolicy overflow = TextOverflowPolicy::Ellipsis;
  std::uint8_t maximumLines = 1;
  float phase = 0.0F;
};

struct BoundedTextLine {
  // Layout results own their rendered ASCII fragments, so they remain valid
  // after the request object goes out of scope. Longer inputs use an explicit
  // overflow policy instead of growing storage dynamically.
  std::array<char, kBoundedTextLineCapacity + 1> value{};
  Rect bounds;
};

struct BoundedTextResult {
  BoundedTextStatus status = BoundedTextStatus::Invalid;
  Rect bounds;
  Rect renderedBounds;
  std::uint8_t scale = 0;
  std::uint8_t lineCount = 0;
  bool truncated = false;
  std::array<BoundedTextLine, kBoundedTextMaxLines> lines{};

  bool drawable() const noexcept {
    return status != BoundedTextStatus::Invalid &&
           status != BoundedTextStatus::NoFit && lineCount > 0 && scale > 0;
  }
};

struct BitmapView {
  const std::uint8_t* data = nullptr;
  std::int32_t width = 0;
  std::int32_t height = 0;
  std::size_t stride = 0;

  bool valid() const noexcept {
    return data && width > 0 && height > 0 &&
           stride >= (static_cast<std::size_t>(width) + 7U) / 8U;
  }
  bool pixel(std::int32_t x, std::int32_t y) const noexcept;
};

enum class BitmapComposition : std::uint8_t {
  Copy,
  SetBlack,
  Xor,
};

struct BitmapBlit {
  BitmapComposition composition = BitmapComposition::Copy;
  bool flipX = false;
  bool flipY = false;
};

struct DitherPattern8x8 {
  std::array<std::uint8_t, 64> thresholds{};

  bool valid() const noexcept;
};

// Cadenza-authored recursive 8x8 ordered threshold table. Values are a
// permutation of [0, 63], so coverage N produces exactly N black pixels per
// complete tile.
extern const DitherPattern8x8 kOrderedDither8x8;

class MonoCanvas {
 public:
  explicit MonoCanvas(MonoFramebuffer& framebuffer,
                      DiagnosticSink* diagnostics = nullptr) noexcept;
  ~MonoCanvas();

  MonoCanvas(const MonoCanvas&) = delete;
  MonoCanvas& operator=(const MonoCanvas&) = delete;
  MonoCanvas(MonoCanvas&&) = delete;
  MonoCanvas& operator=(MonoCanvas&&) = delete;

  std::int16_t width() const noexcept { return framebuffer_.width(); }
  std::int16_t height() const noexcept { return framebuffer_.height(); }
  const Rect& clip() const noexcept { return clip_; }

  bool setClip(Rect clip) noexcept;
  void resetClip() noexcept;
  void clear(bool black = false) noexcept;
  void pixel(std::int32_t x, std::int32_t y, bool black = true) noexcept;
  void line(std::int32_t x0, std::int32_t y0, std::int32_t x1,
            std::int32_t y1, bool black = true) noexcept;
  void rect(std::int32_t x, std::int32_t y, std::int32_t width,
            std::int32_t height, bool black = true) noexcept;
  void fillRect(std::int32_t x, std::int32_t y, std::int32_t width,
                std::int32_t height, bool black = true) noexcept;
  void circle(std::int32_t x, std::int32_t y, std::int32_t radius,
              bool black = true) noexcept;
  void fillCircle(std::int32_t x, std::int32_t y, std::int32_t radius,
                  bool black = true) noexcept;
  TextMetrics measureText(const char* value, std::uint8_t scale = 1) noexcept;
  void text(const char* value, std::int32_t x, std::int32_t y,
            std::uint8_t scale = 1, bool black = true,
            TextAlign align = TextAlign::TopLeft) noexcept;
  BoundedTextResult layoutText(const BoundedTextRequest& request) noexcept;
  bool drawBoundedText(const BoundedTextResult& result,
                       bool black = true) noexcept;
  BoundedTextResult boundedText(const BoundedTextRequest& request,
                                bool black = true) noexcept;
  void drawBitmap(const BitmapView& bitmap, Rect source,
                  std::int32_t destinationX, std::int32_t destinationY,
                  BitmapBlit options = {}) noexcept;
  void drawFramebuffer(const MonoFramebuffer& source, Rect sourceRegion,
                       std::int32_t destinationX, std::int32_t destinationY,
                       BitmapBlit options = {}) noexcept;
  void fillDither(Rect region, const DitherPattern8x8& pattern,
                  std::uint8_t coverage, std::int32_t phaseX = 0,
                  std::int32_t phaseY = 0, bool black = true) noexcept;
  void invert(Rect region) noexcept;

 private:
  static constexpr std::size_t kRasterStateBytes = 512;

  struct RasterState;
  RasterState& raster() noexcept;
  const RasterState& raster() const noexcept;
  void emit(DiagnosticCode code, const char* message) const noexcept;
  bool inClip(std::int32_t x, std::int32_t y) const noexcept;
  void rawPixel(std::int32_t x, std::int32_t y, bool black) noexcept;
  void drawClippedLine(std::int32_t x0, std::int32_t y0, std::int32_t x1,
                       std::int32_t y1, bool black) noexcept;
  void drawCircle(std::int32_t x, std::int32_t y, std::int32_t radius,
                  bool filled, bool black) noexcept;
  void drawTextRaster(const char* value, std::int32_t left,
                      std::int32_t top, std::uint8_t scale, bool black,
                      Rect clip) noexcept;

  MonoFramebuffer& framebuffer_;
  DiagnosticSink* diagnostics_ = nullptr;
  Rect clip_;
  alignas(std::max_align_t) std::byte rasterState_[kRasterStateBytes]{};
};

}  // namespace cadenza
