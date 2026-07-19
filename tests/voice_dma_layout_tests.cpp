#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstdint>
#include <vector>

#include "cadenza/voice/voice_dma_layout.h"

namespace {

using namespace cadenza::voice;

TEST_CASE("layout picker prefers continuous channel over grit-interleaved decode") {
  // Physical mono tone on left, quiet right, packed as int32 MSB.
  std::vector<std::int32_t> words(128);
  for (std::size_t frame = 0; frame < 64; ++frame) {
    const std::int16_t tone =
        static_cast<std::int16_t>((frame % 8 < 4) ? 4000 : -4000);
    words[frame * 2] = static_cast<std::int32_t>(tone) << 16;
    words[frame * 2 + 1] = 0;
  }
  const auto choice = pickBestVoiceLayout(words.data(), words.size() * 4);
  CHECK(choice.layout == VoiceDmaLayout::Int32Msb16);
  CHECK(choice.channel == VoiceDmaChannelMode::First);
  CHECK(choice.quality.score > 0.0F);
}

TEST_CASE("assessMonoChannel punishes high-frequency grit relative to rms") {
  std::array<std::int16_t, 64> clean{};
  std::array<std::int16_t, 64> grit{};
  for (std::size_t i = 0; i < clean.size(); ++i) {
    clean[i] = static_cast<std::int16_t>((i % 16 < 8) ? 2000 : -2000);
    grit[i] = static_cast<std::int16_t>((i % 2 == 0) ? 2000 : -2000);
  }
  const auto cleanQ = assessMonoChannel(clean.data(), clean.size());
  const auto gritQ = assessMonoChannel(grit.data(), grit.size());
  CHECK(cleanQ.score > gritQ.score);
  CHECK(gritQ.hfRatio > cleanQ.hfRatio);
}

}  // namespace
