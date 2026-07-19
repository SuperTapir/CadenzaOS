#pragma once

#include <cstddef>
#include <cstdint>

namespace cadenza {

struct BitmapGlyph {
  std::uint32_t offset = 0;
  std::uint8_t width = 0;
  std::uint8_t stride = 0;
};

struct FontKerningPair {
  std::uint16_t key = 0;
  std::int8_t adjustment = 0;
};

struct BitmapFont {
  const std::uint8_t* data = nullptr;
  std::size_t dataSize = 0;
  const BitmapGlyph* glyphs = nullptr;
  std::size_t glyphCount = 0;
  const FontKerningPair* kerningPairs = nullptr;
  std::size_t kerningCount = 0;
  std::uint8_t firstCodepoint = 0;
  std::uint8_t height = 0;
  std::int8_t ascent = 0;
  std::int8_t descent = 0;
  std::int8_t tracking = 0;

  bool valid() const noexcept;
  const BitmapGlyph* glyph(std::uint8_t codepoint) const noexcept;
  std::int8_t kerning(std::uint8_t first,
                      std::uint8_t second) const noexcept;
};

static_assert(sizeof(BitmapGlyph) == 8,
              "generated font size accounting assumes 8-byte glyphs");
static_assert(sizeof(FontKerningPair) == 4,
              "generated font size accounting assumes 4-byte kerning pairs");

}  // namespace cadenza
