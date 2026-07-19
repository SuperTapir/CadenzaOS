#include "cadenza/core/system_typography.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>

#include "generated/roobert_fonts.h"

namespace cadenza {

bool BitmapFont::valid() const noexcept {
  if (!data || dataSize == 0 || !glyphs || glyphCount == 0 || height == 0 ||
      ascent < 0 || descent > 0 || ascent - descent != height) {
    return false;
  }
  for (std::size_t index = 0; index < glyphCount; ++index) {
    const BitmapGlyph& item = glyphs[index];
    const std::size_t bytes = static_cast<std::size_t>(item.stride) * height;
    if (item.width == 0 || item.stride < (item.width + 7U) / 8U ||
        item.offset > dataSize || bytes > dataSize - item.offset) {
      return false;
    }
  }
  return kerningCount == 0 || kerningPairs;
}

const BitmapGlyph* BitmapFont::glyph(std::uint8_t codepoint) const noexcept {
  if (!valid() || codepoint < firstCodepoint) return nullptr;
  const std::size_t index = codepoint - firstCodepoint;
  return index < glyphCount ? glyphs + index : nullptr;
}

std::int8_t BitmapFont::kerning(std::uint8_t first,
                                std::uint8_t second) const noexcept {
  if (!kerningPairs || kerningCount == 0) return 0;
  const std::uint16_t key = static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(first) << 8U) | second);
  const auto* begin = kerningPairs;
  const auto* end = kerningPairs + kerningCount;
  const auto* found = std::lower_bound(
      begin, end, key,
      [](const FontKerningPair& pair, std::uint16_t requested) {
        return pair.key < requested;
      });
  return found != end && found->key == key ? found->adjustment : 0;
}

bool validTextRole(TextRole role) noexcept {
  switch (role) {
    case TextRole::Hero:
    case TextRole::Title:
    case TextRole::Body:
    case TextRole::Menu:
    case TextRole::Compact:
    case TextRole::Caption:
    case TextRole::Footer:
      return true;
  }
  return false;
}

const BitmapFont& ResolvedTypography::font(TextRole role) const noexcept {
  switch (role) {
    case TextRole::Hero:
      return *hero;
    case TextRole::Title:
      return *title;
    case TextRole::Body:
      return *body;
    case TextRole::Menu:
      return *menu;
    case TextRole::Compact:
      return *compact;
    case TextRole::Caption:
      return *caption;
    case TextRole::Footer:
      return *footer;
  }
  return *caption;
}

TypographyDensity typographyDensityForViewport(std::int32_t width,
                                               std::int32_t height) noexcept {
  // Equivalent to min(width / 400, height / 240) >= 0.9 without floating
  // point. It selects only native, hinted bitmap sizes; glyphs are not
  // fractionally resampled.
  return width > 0 && height > 0 && width * 10 >= 400 * 9 &&
                 height * 10 >= 240 * 9
             ? TypographyDensity::Regular
             : TypographyDensity::Compact;
}

ResolvedTypography resolveTypography(std::int32_t width,
                                     std::int32_t height) noexcept {
  const TypographyDensity density = typographyDensityForViewport(width, height);
  if (density == TypographyDensity::Regular) {
    return {density, &fonts::kRoobert24Medium, &fonts::kRoobert24Medium,
            &fonts::kRoobert20Medium, &fonts::kRoobert11Bold,
            &fonts::kRoobert11Bold, &fonts::kRoobert11Medium,
            &fonts::kRoobert9MonoCondensed};
  }
  return {density, &fonts::kRoobert24Medium, &fonts::kRoobert20Medium,
          &fonts::kRoobert20Medium, &fonts::kRoobert11Bold,
          &fonts::kRoobert10Bold, &fonts::kRoobert11Medium,
          &fonts::kRoobert9MonoCondensed};
}

}  // namespace cadenza
