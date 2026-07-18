#include "cadenza/core/mono_canvas.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <new>

extern "C" {
#include "u8g2.h"
}

#ifndef U8X8_WITH_USER_PTR
#error "Cadenza MonoCanvas requires U8X8_WITH_USER_PTR in every build target"
#endif

namespace cadenza {
namespace {
std::size_t wrap8(std::int64_t value) noexcept {
  const std::int64_t remainder = value % 8;
  return static_cast<std::size_t>(remainder < 0 ? remainder + 8 : remainder);
}

Rect intersection(Rect a, Rect b) noexcept {
  const std::int64_t left = std::max<std::int64_t>(a.x, b.x);
  const std::int64_t top = std::max<std::int64_t>(a.y, b.y);
  const std::int64_t right = std::min<std::int64_t>(
      static_cast<std::int64_t>(a.x) + a.width,
      static_cast<std::int64_t>(b.x) + b.width);
  const std::int64_t bottom = std::min<std::int64_t>(
      static_cast<std::int64_t>(a.y) + a.height,
      static_cast<std::int64_t>(b.y) + b.height);
  if (right <= left || bottom <= top) {
    return {};
  }
  return {static_cast<std::int32_t>(left), static_cast<std::int32_t>(top),
          static_cast<std::int32_t>(right - left),
          static_cast<std::int32_t>(bottom - top)};
}

bool sameRect(Rect a, Rect b) noexcept {
  return a.x == b.x && a.y == b.y && a.width == b.width &&
         a.height == b.height;
}

bool empty(Rect rectangle) noexcept {
  return rectangle.width <= 0 || rectangle.height <= 0;
}

struct ScaledTextDraw {
  MonoFramebuffer* framebuffer = nullptr;
  Rect clip;
  std::int32_t destinationX = 0;
  std::int32_t destinationY = 0;
  std::uint8_t scale = 1;
};

void scaledTextLine(u8g2_t* context, u8g2_uint_t x, u8g2_uint_t y,
                    u8g2_uint_t length, std::uint8_t direction) {
  auto* draw = static_cast<ScaledTextDraw*>(u8g2_GetUserPtr(context));
  if (!draw || !draw->framebuffer) return;
  const bool black = context->draw_color != 0;
  for (u8g2_uint_t index = 0; index < length; ++index) {
    const std::int32_t sourceX = x + (direction == 0 ? index : 0);
    const std::int32_t sourceY = y + (direction == 0 ? 0 : index);
    const std::int32_t left = draw->destinationX + sourceX * draw->scale;
    const std::int32_t top = draw->destinationY + sourceY * draw->scale;
    for (std::uint8_t offsetY = 0; offsetY < draw->scale; ++offsetY) {
      for (std::uint8_t offsetX = 0; offsetX < draw->scale; ++offsetX) {
        const std::int32_t destinationX = left + offsetX;
        const std::int32_t destinationY = top + offsetY;
        if (destinationX >= draw->clip.x && destinationY >= draw->clip.y &&
            destinationX < draw->clip.x + draw->clip.width &&
            destinationY < draw->clip.y + draw->clip.height) {
          draw->framebuffer->setPixel(destinationX, destinationY, black);
        }
      }
    }
  }
}
}  // namespace

const DitherPattern8x8 kOrderedDither8x8{{
    0, 32, 8, 40, 2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44, 4, 36, 14, 46, 6, 38,
    60, 28, 52, 20, 62, 30, 54, 22,
    3, 35, 11, 43, 1, 33, 9, 41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47, 7, 39, 13, 45, 5, 37,
    63, 31, 55, 23, 61, 29, 53, 21,
}};

bool DitherPattern8x8::valid() const noexcept {
  return std::all_of(thresholds.begin(), thresholds.end(),
                     [](std::uint8_t value) { return value < 64; });
}

bool BitmapView::pixel(std::int32_t x, std::int32_t y) const noexcept {
  if (!valid() || x < 0 || y < 0 || x >= width || y >= height) return false;
  const std::size_t offset = static_cast<std::size_t>(y) * stride +
                             static_cast<std::size_t>(x) / 8U;
  return (data[offset] & static_cast<std::uint8_t>(0x80U >> (x & 7))) != 0;
}

struct MonoCanvas::RasterState {
  u8g2_t context{};
  u8x8_display_info_t displayInfo{};
};

MonoCanvas::MonoCanvas(MonoFramebuffer& framebuffer,
                       DiagnosticSink* diagnostics) noexcept
    : framebuffer_(framebuffer), diagnostics_(diagnostics) {
  static_assert(sizeof(RasterState) <= kRasterStateBytes,
                "Increase MonoCanvas opaque raster storage");
  static_assert(alignof(RasterState) <= alignof(std::max_align_t),
                "Increase MonoCanvas opaque raster alignment");
  auto* state = new (rasterState_) RasterState{};
  state->displayInfo.tile_width = static_cast<std::uint8_t>(framebuffer.stride());
  state->displayInfo.tile_height =
      static_cast<std::uint8_t>((framebuffer.height() + 7) / 8);
  state->displayInfo.pixel_width = static_cast<std::uint16_t>(framebuffer.width());
  state->displayInfo.pixel_height = static_cast<std::uint16_t>(framebuffer.height());
  state->context.u8x8.display_info = &state->displayInfo;
  u8g2_SetupBuffer(&state->context, framebuffer.data(),
                   state->displayInfo.tile_height,
                   u8g2_ll_hvline_horizontal_right_lsb, U8G2_R0);
  u8g2_SetFont(&state->context, u8g2_font_6x10_tf);
  resetClip();
}

MonoCanvas::~MonoCanvas() { raster().~RasterState(); }

bool MonoCanvas::setClip(Rect requested) noexcept {
  if (requested.width <= 0 || requested.height <= 0) {
    clip_ = {};
    emit(DiagnosticCode::InvalidGeometry, "invalid clip rectangle");
    return false;
  }
  const Rect bounds{0, 0, framebuffer_.width(), framebuffer_.height()};
  clip_ = intersection(requested, bounds);
  if (empty(clip_)) {
    emit(DiagnosticCode::FullyClipped, "clip outside framebuffer");
    return false;
  }
  if (!sameRect(clip_, requested)) {
    emit(DiagnosticCode::ClippedGeometry, "clip constrained to framebuffer");
  }
  u8g2_SetClipWindow(&raster().context, static_cast<u8g2_uint_t>(clip_.x),
                     static_cast<u8g2_uint_t>(clip_.y),
                     static_cast<u8g2_uint_t>(clip_.x + clip_.width),
                     static_cast<u8g2_uint_t>(clip_.y + clip_.height));
  return true;
}

void MonoCanvas::resetClip() noexcept {
  clip_ = {0, 0, framebuffer_.width(), framebuffer_.height()};
  u8g2_SetClipWindow(&raster().context, 0, 0,
                     static_cast<u8g2_uint_t>(framebuffer_.width()),
                     static_cast<u8g2_uint_t>(framebuffer_.height()));
}

void MonoCanvas::clear(bool black) noexcept { framebuffer_.clear(black); }

void MonoCanvas::pixel(std::int32_t x, std::int32_t y, bool black) noexcept {
  if (!inClip(x, y)) {
    emit(DiagnosticCode::FullyClipped, "pixel outside clip");
    return;
  }
  rawPixel(x, y, black);
}

void MonoCanvas::line(std::int32_t x0, std::int32_t y0, std::int32_t x1,
                      std::int32_t y1, bool black) noexcept {
  const Rect bounds{std::min(x0, x1), std::min(y0, y1),
                    std::max(x0, x1) - std::min(x0, x1) + 1,
                    std::max(y0, y1) - std::min(y0, y1) + 1};
  const Rect visible = intersection(bounds, clip_);
  if (empty(visible)) {
    emit(DiagnosticCode::FullyClipped, "line outside clip");
    return;
  }

  u8g2_SetDrawColor(&raster().context, black ? 1 : 0);
  if (sameRect(bounds, visible)) {
    u8g2_DrawLine(&raster().context, static_cast<u8g2_uint_t>(x0),
                  static_cast<u8g2_uint_t>(y0),
                  static_cast<u8g2_uint_t>(x1),
                  static_cast<u8g2_uint_t>(y1));
  } else {
    emit(DiagnosticCode::ClippedGeometry, "line clipped");
    drawClippedLine(x0, y0, x1, y1, black);
  }
}

void MonoCanvas::rect(std::int32_t x, std::int32_t y, std::int32_t width,
                      std::int32_t height, bool black) noexcept {
  if (width <= 0 || height <= 0) {
    emit(DiagnosticCode::InvalidGeometry, "invalid rectangle");
    return;
  }
  const Rect bounds{x, y, width, height};
  const Rect visible = intersection(bounds, clip_);
  if (empty(visible)) {
    emit(DiagnosticCode::FullyClipped, "rectangle outside clip");
    return;
  }
  u8g2_SetDrawColor(&raster().context, black ? 1 : 0);
  if (sameRect(bounds, visible)) {
    u8g2_DrawFrame(&raster().context, static_cast<u8g2_uint_t>(x),
                   static_cast<u8g2_uint_t>(y),
                   static_cast<u8g2_uint_t>(width),
                   static_cast<u8g2_uint_t>(height));
    return;
  }
  emit(DiagnosticCode::ClippedGeometry, "rectangle clipped");
  drawClippedLine(x, y, x + width - 1, y, black);
  drawClippedLine(x, y + height - 1, x + width - 1, y + height - 1, black);
  drawClippedLine(x, y, x, y + height - 1, black);
  drawClippedLine(x + width - 1, y, x + width - 1, y + height - 1, black);
}

void MonoCanvas::fillRect(std::int32_t x, std::int32_t y, std::int32_t width,
                          std::int32_t height, bool black) noexcept {
  if (width <= 0 || height <= 0) {
    emit(DiagnosticCode::InvalidGeometry, "invalid filled rectangle");
    return;
  }
  const Rect bounds{x, y, width, height};
  const Rect visible = intersection(bounds, clip_);
  if (empty(visible)) {
    emit(DiagnosticCode::FullyClipped, "filled rectangle outside clip");
    return;
  }
  if (!sameRect(bounds, visible)) {
    emit(DiagnosticCode::ClippedGeometry, "filled rectangle clipped");
  }
  u8g2_SetDrawColor(&raster().context, black ? 1 : 0);
  u8g2_DrawBox(&raster().context, static_cast<u8g2_uint_t>(visible.x),
               static_cast<u8g2_uint_t>(visible.y),
               static_cast<u8g2_uint_t>(visible.width),
               static_cast<u8g2_uint_t>(visible.height));
}

void MonoCanvas::circle(std::int32_t x, std::int32_t y, std::int32_t radius,
                        bool black) noexcept {
  drawCircle(x, y, radius, false, black);
}

void MonoCanvas::fillCircle(std::int32_t x, std::int32_t y,
                            std::int32_t radius, bool black) noexcept {
  drawCircle(x, y, radius, true, black);
}

TextMetrics MonoCanvas::measureText(const char* value,
                                    std::uint8_t scale) noexcept {
  if (!value || scale == 0) return {};
  auto& context = raster().context;
  const std::int32_t ascent = u8g2_GetAscent(&context);
  const std::int32_t descent = u8g2_GetDescent(&context);
  return {static_cast<std::int32_t>(u8g2_GetStrWidth(&context, value)) * scale,
          (ascent - descent) * scale, ascent * scale, descent * scale};
}

void MonoCanvas::text(const char* value, std::int32_t x, std::int32_t y,
                      std::uint8_t scale, bool black,
                      TextAlign align) noexcept {
  if (!value || scale == 0) {
    emit(DiagnosticCode::InvalidGeometry, "invalid text or scale");
    return;
  }
  const TextMetrics metrics = measureText(value, scale);
  std::int32_t left = x;
  std::int32_t top = y;
  switch (align) {
    case TextAlign::MiddleLeft:
      top -= metrics.height / 2;
      break;
    case TextAlign::MiddleCenter:
      left -= metrics.width / 2;
      top -= metrics.height / 2;
      break;
    case TextAlign::MiddleRight:
      left -= metrics.width;
      top -= metrics.height / 2;
      break;
    case TextAlign::BottomLeft:
      top -= metrics.height;
      break;
    case TextAlign::BottomRight:
      left -= metrics.width;
      top -= metrics.height;
      break;
    case TextAlign::TopLeft:
      break;
  }

  const Rect bounds{left, top, metrics.width, metrics.height};
  const Rect visible = intersection(bounds, clip_);
  if (empty(visible)) {
    emit(DiagnosticCode::FullyClipped, "text outside clip");
    return;
  }
  if (!sameRect(bounds, visible)) {
    emit(DiagnosticCode::ClippedGeometry, "text clipped");
  }

  auto& context = raster().context;
  ScaledTextDraw draw{&framebuffer_, clip_, left, top, scale};
  const auto previousLine = context.ll_hvline;
  void* previousUser = u8g2_GetUserPtr(&context);
  context.ll_hvline = scaledTextLine;
  u8g2_SetUserPtr(&context, &draw);
  u8g2_SetDrawColor(&context, black ? 1 : 0);
  u8g2_SetMaxClipWindow(&context);
  u8g2_DrawStr(&context, 0,
               static_cast<u8g2_uint_t>(u8g2_GetAscent(&context)), value);
  context.ll_hvline = previousLine;
  u8g2_SetUserPtr(&context, previousUser);
  u8g2_SetClipWindow(&context, static_cast<u8g2_uint_t>(clip_.x),
                     static_cast<u8g2_uint_t>(clip_.y),
                     static_cast<u8g2_uint_t>(clip_.x + clip_.width),
                     static_cast<u8g2_uint_t>(clip_.y + clip_.height));
}

void MonoCanvas::drawBitmap(const BitmapView& bitmap, Rect source,
                            std::int32_t destinationX,
                            std::int32_t destinationY,
                            BitmapBlit options) noexcept {
  if (!bitmap.valid() || source.width <= 0 || source.height <= 0) {
    emit(DiagnosticCode::InvalidGeometry, "invalid bitmap or source rectangle");
    return;
  }
  const Rect bitmapBounds{0, 0, bitmap.width, bitmap.height};
  const Rect clippedSource = intersection(source, bitmapBounds);
  if (empty(clippedSource)) {
    emit(DiagnosticCode::FullyClipped, "bitmap source outside bitmap");
    return;
  }

  destinationX += clippedSource.x - source.x;
  destinationY += clippedSource.y - source.y;
  const Rect destination{destinationX, destinationY, clippedSource.width,
                         clippedSource.height};
  const Rect visible = intersection(destination, clip_);
  if (empty(visible)) {
    emit(DiagnosticCode::FullyClipped, "bitmap destination outside clip");
    return;
  }
  if (!sameRect(source, clippedSource) || !sameRect(destination, visible)) {
    emit(DiagnosticCode::ClippedGeometry, "bitmap blit clipped");
  }

  const auto sourceBegin = reinterpret_cast<std::uintptr_t>(bitmap.data);
  const auto sourceEnd = sourceBegin + bitmap.stride * bitmap.height;
  const auto destinationBegin =
      reinterpret_cast<std::uintptr_t>(framebuffer_.data());
  const auto destinationEnd = destinationBegin + framebuffer_.sizeBytes();
  const bool overlapsStorage =
      sourceBegin < destinationEnd && destinationBegin < sourceEnd;
  if (overlapsStorage && options.flipY) {
    emit(DiagnosticCode::InvalidOperation,
         "overlapping vertically flipped bitmap is unsupported");
    return;
  }

  const bool bottomUp = overlapsStorage && destinationY > clippedSource.y;
  const std::int32_t firstRow = bottomUp ? visible.y + visible.height - 1 : visible.y;
  const std::int32_t lastRow = bottomUp ? visible.y - 1 : visible.y + visible.height;
  const std::int32_t rowStep = bottomUp ? -1 : 1;
  std::array<std::uint8_t, 400> sourceRow{};

  for (std::int32_t y = firstRow; y != lastRow; y += rowStep) {
    const std::int32_t outputY = y - destinationY;
    const std::int32_t sourceY = options.flipY
                                     ? clippedSource.y + clippedSource.height - 1 - outputY
                                     : clippedSource.y + outputY;
    for (std::int32_t x = visible.x; x < visible.x + visible.width; ++x) {
      const std::int32_t outputX = x - destinationX;
      const std::int32_t sourceX = options.flipX
                                       ? clippedSource.x + clippedSource.width - 1 - outputX
                                       : clippedSource.x + outputX;
      sourceRow[static_cast<std::size_t>(x - visible.x)] =
          bitmap.pixel(sourceX, sourceY) ? 1U : 0U;
    }
    for (std::int32_t x = visible.x; x < visible.x + visible.width; ++x) {
      const bool sourceBlack =
          sourceRow[static_cast<std::size_t>(x - visible.x)] != 0;
      switch (options.composition) {
        case BitmapComposition::Copy:
          framebuffer_.setPixel(x, y, sourceBlack);
          break;
        case BitmapComposition::SetBlack:
          if (sourceBlack) framebuffer_.setPixel(x, y, true);
          break;
        case BitmapComposition::Xor:
          if (sourceBlack) framebuffer_.setPixel(x, y, !framebuffer_.pixel(x, y));
          break;
      }
    }
  }
}

void MonoCanvas::drawFramebuffer(const MonoFramebuffer& source,
                                 Rect sourceRegion,
                                 std::int32_t destinationX,
                                 std::int32_t destinationY,
                                 BitmapBlit options) noexcept {
  const BitmapView view{source.data(), source.width(), source.height(),
                        source.stride()};
  drawBitmap(view, sourceRegion, destinationX, destinationY, options);
}

void MonoCanvas::fillDither(Rect region, const DitherPattern8x8& pattern,
                            std::uint8_t coverage, std::int32_t phaseX,
                            std::int32_t phaseY, bool black) noexcept {
  if (region.width <= 0 || region.height <= 0 || coverage > 64 ||
      !pattern.valid()) {
    emit(DiagnosticCode::InvalidGeometry, "invalid dither fill");
    return;
  }
  const Rect visible = intersection(region, clip_);
  if (empty(visible)) {
    emit(DiagnosticCode::FullyClipped, "dither region outside clip");
    return;
  }
  if (!sameRect(region, visible)) {
    emit(DiagnosticCode::ClippedGeometry, "dither region clipped");
  }
  for (std::int32_t y = visible.y; y < visible.y + visible.height; ++y) {
    const std::size_t patternY =
        wrap8(static_cast<std::int64_t>(y) + phaseY);
    for (std::int32_t x = visible.x; x < visible.x + visible.width; ++x) {
      const std::size_t patternX =
          wrap8(static_cast<std::int64_t>(x) + phaseX);
      const bool covered = pattern.thresholds[patternY * 8 + patternX] < coverage;
      framebuffer_.setPixel(x, y, covered ? black : !black);
    }
  }
}

void MonoCanvas::invert(Rect region) noexcept {
  if (region.width <= 0 || region.height <= 0) {
    emit(DiagnosticCode::InvalidGeometry, "invalid inversion region");
    return;
  }
  const Rect visible = intersection(region, clip_);
  if (empty(visible)) {
    emit(DiagnosticCode::FullyClipped, "inversion region outside clip");
    return;
  }
  if (!sameRect(region, visible)) {
    emit(DiagnosticCode::ClippedGeometry, "inversion region clipped");
  }
  for (std::int32_t y = visible.y; y < visible.y + visible.height; ++y) {
    for (std::int32_t x = visible.x; x < visible.x + visible.width; ++x) {
      framebuffer_.setPixel(x, y, !framebuffer_.pixel(x, y));
    }
  }
}

MonoCanvas::RasterState& MonoCanvas::raster() noexcept {
  return *std::launder(reinterpret_cast<RasterState*>(rasterState_));
}

const MonoCanvas::RasterState& MonoCanvas::raster() const noexcept {
  return *std::launder(reinterpret_cast<const RasterState*>(rasterState_));
}

void MonoCanvas::emit(DiagnosticCode code, const char* message) const noexcept {
  if (diagnostics_) {
    diagnostics_->emit(
        {DiagnosticCategory::Graphics, code, message, 0});
  }
}

bool MonoCanvas::inClip(std::int32_t x, std::int32_t y) const noexcept {
  return x >= clip_.x && y >= clip_.y && x < clip_.x + clip_.width &&
         y < clip_.y + clip_.height;
}

void MonoCanvas::rawPixel(std::int32_t x, std::int32_t y, bool black) noexcept {
  if (inClip(x, y)) {
    framebuffer_.setPixel(x, y, black);
  }
}

void MonoCanvas::drawClippedLine(std::int32_t x0, std::int32_t y0,
                                 std::int32_t x1, std::int32_t y1,
                                 bool black) noexcept {
  const std::int32_t dx = std::abs(x1 - x0);
  const std::int32_t stepX = x0 < x1 ? 1 : -1;
  const std::int32_t dy = -std::abs(y1 - y0);
  const std::int32_t stepY = y0 < y1 ? 1 : -1;
  std::int32_t error = dx + dy;
  while (true) {
    rawPixel(x0, y0, black);
    if (x0 == x1 && y0 == y1) break;
    const std::int32_t doubled = error * 2;
    if (doubled >= dy) {
      error += dy;
      x0 += stepX;
    }
    if (doubled <= dx) {
      error += dx;
      y0 += stepY;
    }
  }
}

void MonoCanvas::drawCircle(std::int32_t centerX, std::int32_t centerY,
                            std::int32_t radius, bool filled,
                            bool black) noexcept {
  if (radius < 0) {
    emit(DiagnosticCode::InvalidGeometry, "negative circle radius");
    return;
  }
  const Rect bounds{centerX - radius, centerY - radius, radius * 2 + 1,
                    radius * 2 + 1};
  const Rect visible = intersection(bounds, clip_);
  if (empty(visible)) {
    emit(DiagnosticCode::FullyClipped, "circle outside clip");
    return;
  }
  if (!sameRect(bounds, visible)) {
    emit(DiagnosticCode::ClippedGeometry, "circle clipped");
  }

  if (sameRect(bounds, visible)) {
    u8g2_SetDrawColor(&raster().context, black ? 1 : 0);
    if (filled) {
      u8g2_DrawDisc(&raster().context, static_cast<u8g2_uint_t>(centerX),
                    static_cast<u8g2_uint_t>(centerY),
                    static_cast<u8g2_uint_t>(radius), U8G2_DRAW_ALL);
    } else {
      u8g2_DrawCircle(&raster().context, static_cast<u8g2_uint_t>(centerX),
                      static_cast<u8g2_uint_t>(centerY),
                      static_cast<u8g2_uint_t>(radius), U8G2_DRAW_ALL);
    }
    return;
  }

  std::int32_t f = 1 - radius;
  std::int32_t deltaX = 1;
  std::int32_t deltaY = -2 * radius;
  std::int32_t x = 0;
  std::int32_t y = radius;
  const auto section = [&](std::int32_t sectionX, std::int32_t sectionY) {
    if (filled) {
      drawClippedLine(centerX + sectionX, centerY - sectionY,
                      centerX + sectionX, centerY + sectionY, black);
      drawClippedLine(centerX - sectionX, centerY - sectionY,
                      centerX - sectionX, centerY + sectionY, black);
      drawClippedLine(centerX + sectionY, centerY - sectionX,
                      centerX + sectionY, centerY + sectionX, black);
      drawClippedLine(centerX - sectionY, centerY - sectionX,
                      centerX - sectionY, centerY + sectionX, black);
    } else {
      rawPixel(centerX + sectionX, centerY - sectionY, black);
      rawPixel(centerX + sectionY, centerY - sectionX, black);
      rawPixel(centerX - sectionX, centerY - sectionY, black);
      rawPixel(centerX - sectionY, centerY - sectionX, black);
      rawPixel(centerX + sectionX, centerY + sectionY, black);
      rawPixel(centerX + sectionY, centerY + sectionX, black);
      rawPixel(centerX - sectionX, centerY + sectionY, black);
      rawPixel(centerX - sectionY, centerY + sectionX, black);
    }
  };
  section(x, y);
  while (x < y) {
    if (f >= 0) {
      --y;
      deltaY += 2;
      f += deltaY;
    }
    ++x;
    deltaX += 2;
    f += deltaX;
    section(x, y);
  }
}

}  // namespace cadenza
