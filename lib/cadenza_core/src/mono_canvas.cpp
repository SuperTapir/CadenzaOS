#include "cadenza/core/mono_canvas.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
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

bool contains(Rect outer, Rect inner) noexcept {
  if (empty(outer) || empty(inner)) return false;
  const std::int64_t outerRight =
      static_cast<std::int64_t>(outer.x) + outer.width;
  const std::int64_t outerBottom =
      static_cast<std::int64_t>(outer.y) + outer.height;
  const std::int64_t innerRight =
      static_cast<std::int64_t>(inner.x) + inner.width;
  const std::int64_t innerBottom =
      static_cast<std::int64_t>(inner.y) + inner.height;
  return inner.x >= outer.x && inner.y >= outer.y &&
         innerRight <= outerRight && innerBottom <= outerBottom;
}

Rect unite(Rect first, Rect second) noexcept {
  if (empty(first)) return second;
  if (empty(second)) return first;
  const std::int64_t left = std::min<std::int64_t>(first.x, second.x);
  const std::int64_t top = std::min<std::int64_t>(first.y, second.y);
  const std::int64_t right = std::max<std::int64_t>(
      static_cast<std::int64_t>(first.x) + first.width,
      static_cast<std::int64_t>(second.x) + second.width);
  const std::int64_t bottom = std::max<std::int64_t>(
      static_cast<std::int64_t>(first.y) + first.height,
      static_cast<std::int64_t>(second.y) + second.height);
  return {static_cast<std::int32_t>(left), static_cast<std::int32_t>(top),
          static_cast<std::int32_t>(right - left),
          static_cast<std::int32_t>(bottom - top)};
}

std::size_t boundedLength(const char* value, std::size_t limit) noexcept {
  if (!value) return 0;
  std::size_t length = 0;
  while (length < limit && value[length] != '\0') ++length;
  return length;
}

bool leftAligned(TextAlign align) noexcept {
  return align == TextAlign::TopLeft || align == TextAlign::MiddleLeft ||
         align == TextAlign::BottomLeft;
}

bool rightAligned(TextAlign align) noexcept {
  return align == TextAlign::MiddleRight || align == TextAlign::BottomRight;
}

bool topAligned(TextAlign align) noexcept {
  return align == TextAlign::TopLeft;
}

bool bottomAligned(TextAlign align) noexcept {
  return align == TextAlign::BottomLeft || align == TextAlign::BottomRight;
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
    : framebuffer_(framebuffer),
      diagnostics_(diagnostics),
      typography_(resolveTypography(framebuffer.width(), framebuffer.height())) {
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
  resetClip();
}

MonoCanvas::~MonoCanvas() { raster().~RasterState(); }

bool MonoCanvas::setClip(Rect requested, bool reportGeometryClips) noexcept {
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
  reportGeometryClips_ = reportGeometryClips;
  return true;
}

void MonoCanvas::resetClip() noexcept {
  reportGeometryClips_ = true;
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

void MonoCanvas::fillRoundedRect(std::int32_t x, std::int32_t y,
                                 std::int32_t width, std::int32_t height,
                                 std::int32_t radius, bool black) noexcept {
  if (width <= 0 || height <= 0 || radius < 0) {
    emit(DiagnosticCode::InvalidGeometry, "invalid rounded rectangle");
    return;
  }
  const std::int32_t resolvedRadius =
      std::min(radius, std::min(width, height) / 2);
  if (resolvedRadius == 0) {
    fillRect(x, y, width, height, black);
    return;
  }
  if (width - resolvedRadius * 2 > 0) {
    fillRect(x + resolvedRadius, y, width - resolvedRadius * 2, height,
             black);
  }
  if (height - resolvedRadius * 2 > 0) {
    fillRect(x, y + resolvedRadius, width, height - resolvedRadius * 2,
             black);
  }
  fillCircle(x + resolvedRadius, y + resolvedRadius, resolvedRadius, black);
  fillCircle(x + width - resolvedRadius - 1, y + resolvedRadius,
             resolvedRadius, black);
  fillCircle(x + resolvedRadius, y + height - resolvedRadius - 1,
             resolvedRadius, black);
  fillCircle(x + width - resolvedRadius - 1,
             y + height - resolvedRadius - 1, resolvedRadius, black);
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
  return measureText(value, TextRole::Caption, scale);
}

TextMetrics MonoCanvas::measureText(const char* value, TextRole role,
                                    std::uint8_t scale) noexcept {
  if (!value || scale == 0 || !validTextRole(role)) return {};
  const BitmapFont& font = typography_.font(role);
  if (!font.valid()) return {};
  std::int32_t width = 0;
  for (std::size_t index = 0; value[index] != '\0'; ++index) {
    std::uint8_t codepoint = static_cast<std::uint8_t>(value[index]);
    const BitmapGlyph* glyph = font.glyph(codepoint);
    if (!glyph) {
      codepoint = static_cast<std::uint8_t>('?');
      glyph = font.glyph(codepoint);
    }
    if (!glyph) continue;
    width += glyph->width;
    if (value[index + 1] != '\0') {
      std::uint8_t next = static_cast<std::uint8_t>(value[index + 1]);
      if (!font.glyph(next)) next = static_cast<std::uint8_t>('?');
      width += font.tracking + font.kerning(codepoint, next);
    }
  }
  return {width * scale, static_cast<std::int32_t>(font.height) * scale,
          static_cast<std::int32_t>(font.ascent) * scale,
          static_cast<std::int32_t>(font.descent) * scale};
}

void MonoCanvas::text(const char* value, std::int32_t x, std::int32_t y,
                      std::uint8_t scale, bool black,
                      TextAlign align, TextRole role) noexcept {
  if (!value || scale == 0 || !validTextRole(role)) {
    emit(DiagnosticCode::InvalidGeometry, "invalid text or scale");
    return;
  }
  const TextMetrics metrics = measureText(value, role, scale);
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

  drawTextRaster(value, left, top, scale, black, clip_, role);
}

BoundedTextResult MonoCanvas::layoutText(
    const BoundedTextRequest& request) noexcept {
  BoundedTextResult result;
  result.bounds = request.bounds;
  result.role = request.role;
  if (!request.value || request.bounds.width <= 0 ||
      request.bounds.height <= 0 || request.preferredScale == 0 ||
      request.minimumScale == 0 ||
      request.minimumScale > request.preferredScale ||
      request.maximumLines == 0 || !std::isfinite(request.phase) ||
      !validTextRole(request.role) ||
      (reportGeometryClips_ && !contains(clip_, request.bounds))) {
    emit(DiagnosticCode::InvalidGeometry, "invalid bounded text request");
    return result;
  }

  if (!reportGeometryClips_ && empty(intersection(clip_, request.bounds))) {
    result.status = BoundedTextStatus::NoFit;
    return result;
  }

  if (request.value[0] == '\0') {
    result.status = BoundedTextStatus::NoFit;
    return result;
  }

  const std::size_t inputLength =
      boundedLength(request.value, kBoundedTextLineCapacity + 1);
  const bool inputFitsStorage = inputLength <= kBoundedTextLineCapacity;

  auto copyRange = [](BoundedTextLine& line, const char* source,
                      std::size_t length) noexcept {
    length = std::min(length, kBoundedTextLineCapacity);
    if (length > 0) std::memcpy(line.value.data(), source, length);
    line.value[length] = '\0';
  };

  auto placeLines = [&](std::uint8_t lineCount, std::uint8_t scale,
                        bool marquee = false) noexcept {
    result.lineCount = lineCount;
    result.scale = scale;
    const TextMetrics baseMetrics = measureText("A", request.role, scale);
    const std::int32_t lineHeight = baseMetrics.height;
    const std::int32_t blockHeight = lineHeight * lineCount;
    std::int32_t top = request.bounds.y;
    if (bottomAligned(request.align)) {
      top = request.bounds.y + request.bounds.height - blockHeight;
    } else if (!topAligned(request.align)) {
      top = request.bounds.y + (request.bounds.height - blockHeight) / 2;
    }
    result.renderedBounds = {};
    for (std::uint8_t index = 0; index < lineCount; ++index) {
      BoundedTextLine& line = result.lines[index];
      const TextMetrics metrics =
          measureText(line.value.data(), request.role, scale);
      std::int32_t left = request.bounds.x;
      if (rightAligned(request.align)) {
        left = request.bounds.x + request.bounds.width - metrics.width;
      } else if (!leftAligned(request.align)) {
        left = request.bounds.x + (request.bounds.width - metrics.width) / 2;
      }
      if (marquee) {
        const float phase = std::max(0.0F, std::min(1.0F, request.phase));
        const std::int32_t travel =
            std::max<std::int32_t>(0, metrics.width - request.bounds.width);
        left = request.bounds.x -
               static_cast<std::int32_t>(std::lround(phase * travel));
      }
      line.bounds = {left, top + index * lineHeight, metrics.width,
                     metrics.height};
      result.renderedBounds =
          unite(result.renderedBounds, intersection(line.bounds, request.bounds));
    }
  };

  if (inputFitsStorage) {
    for (std::int32_t scale = request.preferredScale;
         scale >= request.minimumScale; --scale) {
      const TextMetrics metrics =
          measureText(request.value, request.role,
                      static_cast<std::uint8_t>(scale));
      if (metrics.width <= request.bounds.width &&
          metrics.height <= request.bounds.height) {
        copyRange(result.lines[0], request.value, inputLength);
        placeLines(1, static_cast<std::uint8_t>(scale));
        result.status = scale == request.preferredScale
                            ? BoundedTextStatus::Complete
                            : BoundedTextStatus::Scaled;
        return result;
      }
    }
  }

  const std::uint8_t fallbackScale = request.minimumScale;
  const TextMetrics fallbackMetrics =
      measureText("A", request.role, fallbackScale);
  if (fallbackMetrics.height > request.bounds.height) {
    result.status = BoundedTextStatus::NoFit;
    return result;
  }

  auto ellipsize = [&](BoundedTextLine& line, const char* source,
                       std::size_t sourceLength) noexcept -> bool {
    constexpr char kEllipsis[] = "...";
    const std::int32_t ellipsisWidth =
        measureText(kEllipsis, request.role, fallbackScale).width;
    if (ellipsisWidth > request.bounds.width) return false;
    const std::size_t maximumPrefix =
        std::min(sourceLength, kBoundedTextLineCapacity - 3);
    std::size_t chosen = 0;
    for (std::size_t length = 0; length <= maximumPrefix; ++length) {
      copyRange(line, source, length);
      std::memcpy(line.value.data() + length, kEllipsis, 4);
      if (measureText(line.value.data(), request.role, fallbackScale).width <=
          request.bounds.width) {
        chosen = length;
      } else {
        break;
      }
    }
    copyRange(line, source, chosen);
    std::memcpy(line.value.data() + chosen, kEllipsis, 4);
    return true;
  };

  if (request.overflow == TextOverflowPolicy::Ellipsis) {
    if (!ellipsize(result.lines[0], request.value, inputLength)) {
      result.status = BoundedTextStatus::NoFit;
      return result;
    }
    result.truncated = true;
    placeLines(1, fallbackScale);
    result.status = BoundedTextStatus::Truncated;
    return result;
  }

  if (request.overflow == TextOverflowPolicy::Marquee) {
    if (!inputFitsStorage) {
      emit(DiagnosticCode::CapacityExceeded,
           "bounded marquee text exceeds fixed storage");
      result.status = BoundedTextStatus::NoFit;
      return result;
    }
    const TextMetrics metrics =
        measureText(request.value, request.role, request.preferredScale);
    if (metrics.height > request.bounds.height) {
      result.status = BoundedTextStatus::NoFit;
      return result;
    }
    copyRange(result.lines[0], request.value, inputLength);
    placeLines(1, request.preferredScale, true);
    result.status = BoundedTextStatus::Marquee;
    return result;
  }

  const std::uint8_t verticalLines = static_cast<std::uint8_t>(
      request.bounds.height /
      std::max<std::int32_t>(1, fallbackMetrics.height));
  const std::uint8_t lineLimit = std::min<std::uint8_t>(
      {request.maximumLines, verticalLines,
       static_cast<std::uint8_t>(kBoundedTextMaxLines)});
  if (lineLimit == 0) {
    result.status = BoundedTextStatus::NoFit;
    return result;
  }

  // Wrapping scans a bounded prefix. Inputs beyond the fixed result capacity
  // are still represented safely by ellipsizing the final visible line.
  constexpr std::size_t kWrapScanCapacity = 512;
  const std::size_t scannedLength = boundedLength(request.value, kWrapScanCapacity);
  const bool scanTruncated = scannedLength == kWrapScanCapacity &&
                             request.value[scannedLength] != '\0';
  std::size_t position = 0;
  std::uint8_t lineCount = 0;
  bool contentRemaining = scanTruncated;
  while (position < scannedLength && lineCount < lineLimit) {
    while (position < scannedLength && request.value[position] == ' ') ++position;
    if (position >= scannedLength) break;
    if (lineCount + 1 == lineLimit) {
      const std::size_t remaining = scannedLength - position;
      if (!scanTruncated && remaining <= kBoundedTextLineCapacity &&
          measureText(request.value + position, request.role, fallbackScale)
                  .width <=
              request.bounds.width) {
        copyRange(result.lines[lineCount], request.value + position, remaining);
        position = scannedLength;
      } else {
        if (!ellipsize(result.lines[lineCount], request.value + position,
                       remaining)) {
          result.status = BoundedTextStatus::NoFit;
          return result;
        }
        contentRemaining = true;
      }
      ++lineCount;
      break;
    }

    std::array<char, kBoundedTextLineCapacity + 1> candidate{};
    std::size_t fitted = 0;
    std::size_t lastSpace = 0;
    bool sawSpace = false;
    bool explicitBreak = false;
    while (position + fitted < scannedLength &&
           fitted < kBoundedTextLineCapacity) {
      const char next = request.value[position + fitted];
      if (next == '\n') {
        explicitBreak = true;
        break;
      }
      candidate[fitted] = next;
      candidate[fitted + 1] = '\0';
      if (measureText(candidate.data(), request.role, fallbackScale).width >
          request.bounds.width) {
        candidate[fitted] = '\0';
        break;
      }
      ++fitted;
      if (next == ' ') {
        lastSpace = fitted - 1;
        sawSpace = true;
      }
    }
    if (fitted == 0 && !explicitBreak) {
      result.status = BoundedTextStatus::NoFit;
      return result;
    }
    std::size_t take = fitted;
    std::size_t nextPosition = position + fitted;
    if (explicitBreak) {
      nextPosition = position + fitted + 1;
    } else if (position + fitted < scannedLength && sawSpace && lastSpace > 0) {
      take = lastSpace;
      nextPosition = position + lastSpace + 1;
    }
    while (take > 0 && request.value[position + take - 1] == ' ') --take;
    copyRange(result.lines[lineCount], request.value + position, take);
    ++lineCount;
    position = nextPosition;
  }
  while (position < scannedLength && request.value[position] == ' ') ++position;
  contentRemaining = contentRemaining || position < scannedLength;
  if (lineCount == 0) {
    result.status = BoundedTextStatus::NoFit;
    return result;
  }
  if (contentRemaining) {
    BoundedTextLine& finalLine = result.lines[lineCount - 1];
    const std::size_t finalLength = std::strlen(finalLine.value.data());
    std::array<char, kBoundedTextLineCapacity + 1> original = finalLine.value;
    if (!ellipsize(finalLine, original.data(), finalLength)) {
      result.status = BoundedTextStatus::NoFit;
      return result;
    }
  }
  result.truncated = contentRemaining;
  placeLines(lineCount, fallbackScale);
  result.status = BoundedTextStatus::Wrapped;
  return result;
}

bool MonoCanvas::drawBoundedText(const BoundedTextResult& result,
                                 bool black) noexcept {
  if (!result.drawable()) return false;
  if ((reportGeometryClips_ && !contains(clip_, result.bounds)) ||
      result.lineCount > kBoundedTextMaxLines ||
      !contains(result.bounds, result.renderedBounds)) {
    emit(DiagnosticCode::InvalidGeometry, "invalid bounded text result");
    return false;
  }
  const Rect textClip = reportGeometryClips_
                            ? result.bounds
                            : intersection(result.bounds, clip_);
  if (empty(textClip)) return false;
  for (std::uint8_t index = 0; index < result.lineCount; ++index) {
    const BoundedTextLine& line = result.lines[index];
    if (line.value[0] == '\0') continue;
    if (result.status != BoundedTextStatus::Marquee &&
        !contains(result.bounds, line.bounds)) {
      emit(DiagnosticCode::InvalidGeometry, "bounded text line escaped bounds");
      return false;
    }
    drawTextRaster(line.value.data(), line.bounds.x, line.bounds.y,
                   result.scale, black, textClip, result.role);
  }
  return true;
}

BoundedTextResult MonoCanvas::boundedText(const BoundedTextRequest& request,
                                          bool black) noexcept {
  BoundedTextResult result = layoutText(request);
  drawBoundedText(result, black);
  return result;
}

void MonoCanvas::drawTextRaster(const char* value, std::int32_t left,
                                std::int32_t top, std::uint8_t scale,
                                bool black, Rect clip, TextRole role) noexcept {
  if (!value || scale == 0 || empty(clip) || !validTextRole(role)) return;
  const BitmapFont& font = typography_.font(role);
  if (!font.valid()) return;

  std::int32_t cursor = left;
  for (std::size_t index = 0; value[index] != '\0'; ++index) {
    std::uint8_t codepoint = static_cast<std::uint8_t>(value[index]);
    const BitmapGlyph* glyph = font.glyph(codepoint);
    if (!glyph) {
      codepoint = static_cast<std::uint8_t>('?');
      glyph = font.glyph(codepoint);
    }
    if (!glyph) continue;
    const std::uint8_t* glyphData = font.data + glyph->offset;
    for (std::int32_t sourceY = 0; sourceY < font.height; ++sourceY) {
      for (std::int32_t sourceX = 0; sourceX < glyph->width; ++sourceX) {
        const std::size_t offset =
            static_cast<std::size_t>(sourceY) * glyph->stride +
            static_cast<std::size_t>(sourceX) / 8U;
        if ((glyphData[offset] &
             static_cast<std::uint8_t>(0x80U >> (sourceX & 7))) == 0) {
          continue;
        }
        for (std::int32_t dy = 0; dy < scale; ++dy) {
          for (std::int32_t dx = 0; dx < scale; ++dx) {
            const std::int32_t x = cursor + sourceX * scale + dx;
            const std::int32_t y = top + sourceY * scale + dy;
            if (x >= clip.x && y >= clip.y && x < clip.x + clip.width &&
                y < clip.y + clip.height) {
              rawPixel(x, y, black);
            }
          }
        }
      }
    }
    cursor += static_cast<std::int32_t>(glyph->width) * scale;
    if (value[index + 1] != '\0') {
      std::uint8_t next = static_cast<std::uint8_t>(value[index + 1]);
      if (!font.glyph(next)) next = static_cast<std::uint8_t>('?');
      cursor += static_cast<std::int32_t>(font.tracking +
                                          font.kerning(codepoint, next)) *
                scale;
    }
  }
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
  if (!reportGeometryClips_ &&
      (code == DiagnosticCode::ClippedGeometry ||
       code == DiagnosticCode::FullyClipped)) {
    return;
  }
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
