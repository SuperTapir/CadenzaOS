#pragma once

#include <cstddef>
#include <cstdint>

namespace cadenza::audio {

// ESP-IDF's I2S_CHANNEL_FMT_RIGHT_LEFT consumes one right sample followed by
// one left sample. Cadenza's engine is intentionally mono, so the firmware
// adapter duplicates each sample without changing the core render contract.
struct StereoI2sFrame {
  std::int16_t right = 0;
  std::int16_t left = 0;
};

static_assert(sizeof(StereoI2sFrame) == sizeof(std::int16_t) * 2,
              "I2S stereo frames must not contain padding");

inline std::size_t duplicateMonoToStereo(const std::int16_t* mono,
                                         StereoI2sFrame* stereo,
                                         std::size_t sampleCount) noexcept {
  if (mono == nullptr || stereo == nullptr) return 0;
  for (std::size_t index = 0; index < sampleCount; ++index) {
    stereo[index].right = mono[index];
    stereo[index].left = mono[index];
  }
  return sampleCount;
}

}  // namespace cadenza::audio
