#pragma once

// Official pin mapping for the original LILYGO T-Embed (not CC1101).
namespace BoardPins {
constexpr int kPowerOn = 46;

constexpr int kEncoderA = 2;
constexpr int kEncoderB = 1;
constexpr int kEncoderButton = 0;

constexpr int kLcdBacklight = 15;

constexpr int kI2sBclk = 7;
constexpr int kI2sWclk = 5;
constexpr int kI2sDataOut = 6;
}  // namespace BoardPins
