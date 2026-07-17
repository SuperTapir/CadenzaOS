#pragma once

#include <Arduino.h>

struct DisplayProfile {
  const char* name;
  int16_t width;
  int16_t height;
  bool reflective;
};

namespace DisplayProfiles {
constexpr DisplayProfile kTEmbed{"T-Embed ST7789", 320, 170, false};
constexpr DisplayProfile kSharpMemoryLcd{"Sharp LS027B7DH01", 400, 240, true};
}  // namespace DisplayProfiles
