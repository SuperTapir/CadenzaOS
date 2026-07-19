#pragma once

#include <cstdint>

#include "cadenza/core/bitmap_font.h"

namespace cadenza {

enum class TextRole : std::uint8_t {
  Hero,
  Title,
  Body,
  Menu,
  Compact,
  Caption,
  Footer,
};

enum class TypographyDensity : std::uint8_t {
  Compact,
  Regular,
};

struct ResolvedTypography {
  TypographyDensity density = TypographyDensity::Compact;
  const BitmapFont* hero = nullptr;
  const BitmapFont* title = nullptr;
  const BitmapFont* body = nullptr;
  const BitmapFont* menu = nullptr;
  const BitmapFont* compact = nullptr;
  const BitmapFont* caption = nullptr;
  const BitmapFont* footer = nullptr;

  const BitmapFont& font(TextRole role) const noexcept;
};

bool validTextRole(TextRole role) noexcept;
TypographyDensity typographyDensityForViewport(std::int32_t width,
                                               std::int32_t height) noexcept;
ResolvedTypography resolveTypography(std::int32_t width,
                                     std::int32_t height) noexcept;

}  // namespace cadenza
