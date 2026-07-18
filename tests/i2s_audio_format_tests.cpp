#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstdint>

#include "board_pins.h"
#include "cadenza/audio/i2s_audio_format.h"

TEST_CASE("T-Embed audio pins match the original board") {
  CHECK(BoardPins::kI2sBclk == 7);
  CHECK(BoardPins::kI2sWclk == 5);
  CHECK(BoardPins::kI2sDataOut == 6);
}

TEST_CASE("mono PCM is duplicated into right-left I2S frames") {
  const std::array<std::int16_t, 5> mono{{0, 1, -1, 32767, -32768}};
  std::array<cadenza::audio::StereoI2sFrame, mono.size()> frames{};
  REQUIRE(cadenza::audio::duplicateMonoToStereo(
              mono.data(), frames.data(), mono.size()) == mono.size());
  for (std::size_t index = 0; index < mono.size(); ++index) {
    CHECK(frames[index].right == mono[index]);
    CHECK(frames[index].left == mono[index]);
  }
}

TEST_CASE("mono duplication rejects null buffers") {
  cadenza::audio::StereoI2sFrame frame{};
  std::int16_t sample = 1;
  CHECK(cadenza::audio::duplicateMonoToStereo(nullptr, &frame, 1) == 0);
  CHECK(cadenza::audio::duplicateMonoToStereo(&sample, nullptr, 1) == 0);
}
