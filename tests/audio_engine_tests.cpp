#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include "cadenza/audio/audio_engine.h"

namespace {
cadenza::audio::ToneSpec tone(cadenza::audio::Waveform waveform,
                              float frequency = 441.0F,
                              float duration = 0.01F,
                              std::uint8_t priority = 1) {
  cadenza::audio::ToneSpec spec;
  spec.waveform = waveform;
  spec.startFrequencyHz = frequency;
  spec.endFrequencyHz = frequency;
  spec.attackSeconds = 8.0F / cadenza::audio::kSampleRate;
  spec.durationSeconds = duration;
  spec.releaseSeconds = 8.0F / cadenza::audio::kSampleRate;
  spec.gain = 1.0F;
  spec.priority = priority;
  return spec;
}

std::int32_t largestAdjacentJump(const std::int16_t* samples,
                                 std::size_t count) {
  std::int32_t largest = 0;
  for (std::size_t index = 1; index < count; ++index) {
    largest = std::max(
        largest, std::abs(static_cast<std::int32_t>(samples[index]) -
                          static_cast<std::int32_t>(samples[index - 1])));
  }
  return largest;
}
}  // namespace

TEST_CASE("idle audio engine renders exact silence") {
  cadenza::audio::AudioEngine engine;
  std::array<std::int16_t, 64> samples;
  samples.fill(1234);
  engine.render(samples.data(), samples.size());
  CHECK(std::all_of(samples.begin(), samples.end(),
                    [](std::int16_t sample) { return sample == 0; }));
}

TEST_CASE("triangle tone has a ramped start and returns to exact silence") {
  cadenza::audio::AudioEngine engine;
  REQUIRE(engine.play(tone(cadenza::audio::Waveform::Triangle)));
  std::array<std::int16_t, 520> samples{};
  engine.render(samples.data(), samples.size());

  CHECK(samples.front() == 0);
  CHECK(std::count_if(samples.begin(), samples.end(),
                      [](std::int16_t sample) { return sample != 0; }) > 300);
  CHECK(std::all_of(samples.begin() + 442, samples.end(),
                    [](std::int16_t sample) { return sample == 0; }));
  CHECK(engine.activeVoiceCount() == 0);
}

TEST_CASE("PolyBLEP square softens the phase discontinuity") {
  cadenza::audio::AudioEngine engine;
  auto spec = tone(cadenza::audio::Waveform::Square, 441.0F, 0.05F);
  REQUIRE(engine.play(spec));
  std::array<std::int16_t, 110> samples{};
  engine.render(samples.data(), samples.size());

  const auto plateau = std::abs(static_cast<int>(samples[40]));
  int transitionMinimum = plateau;
  for (std::size_t index = 48; index <= 52; ++index) {
    transitionMinimum =
        std::min(transitionMinimum, std::abs(static_cast<int>(samples[index])));
  }
  REQUIRE(plateau > 10000);
  CHECK(transitionMinimum < plateau / 2);
}

TEST_CASE("noise rendering is deterministic from a fixed seed") {
  cadenza::audio::AudioEngine first{0x12345678U};
  cadenza::audio::AudioEngine second{0x12345678U};
  const auto spec = tone(cadenza::audio::Waveform::Noise, 900.0F, 0.02F);
  REQUIRE(first.play(spec));
  REQUIRE(second.play(spec));
  std::array<std::int16_t, 512> a{};
  std::array<std::int16_t, 512> b{};
  first.render(a.data(), a.size());
  second.render(b.data(), b.size());
  CHECK(a == b);
}

TEST_CASE("invalid tone parameters cannot reach the sample loop") {
  cadenza::audio::AudioEngine engine;
  auto invalid = tone(cadenza::audio::Waveform::Triangle);
  invalid.startFrequencyHz = std::numeric_limits<float>::quiet_NaN();
  CHECK_FALSE(engine.play(invalid));
  invalid = tone(cadenza::audio::Waveform::Triangle);
  invalid.endFrequencyHz = -1.0F;
  CHECK_FALSE(engine.play(invalid));
  invalid = tone(cadenza::audio::Waveform::Triangle);
  invalid.durationSeconds = std::numeric_limits<float>::infinity();
  CHECK_FALSE(engine.play(invalid));
  CHECK(engine.activeVoiceCount() == 0);
}

TEST_CASE("three high gain voices saturate without integer wrap") {
  cadenza::audio::AudioEngine engine;
  auto spec = tone(cadenza::audio::Waveform::Triangle, 220.5F, 0.05F, 1);
  spec.gain = 4.0F;
  REQUIRE(engine.play(spec));
  REQUIRE(engine.play(spec));
  REQUIRE(engine.play(spec));
  std::array<std::int16_t, 256> samples{};
  engine.render(samples.data(), samples.size());
  const auto [minimum, maximum] =
      std::minmax_element(samples.begin(), samples.end());
  CHECK(*maximum == std::numeric_limits<std::int16_t>::max());
  CHECK(*minimum == std::numeric_limits<std::int16_t>::min());
}

TEST_CASE("voice stealing chooses oldest lowest priority and releases it") {
  cadenza::audio::AudioEngine engine;
  REQUIRE(engine.play(tone(cadenza::audio::Waveform::Triangle, 400.0F, 0.2F, 2)));
  REQUIRE(engine.play(tone(cadenza::audio::Waveform::Triangle, 500.0F, 0.2F, 1)));
  REQUIRE(engine.play(tone(cadenza::audio::Waveform::Triangle, 600.0F, 0.2F, 1)));
  std::array<std::int16_t, 32> warmup{};
  engine.render(warmup.data(), warmup.size());

  CHECK_FALSE(engine.play(tone(cadenza::audio::Waveform::Noise, 700.0F, 0.1F, 0)));
  REQUIRE(engine.play(
      tone(cadenza::audio::Waveform::Triangle, 700.0F, 0.1F, 3)));
  const auto first = engine.voiceInfo(0);
  const auto second = engine.voiceInfo(1);
  const auto third = engine.voiceInfo(2);
  CHECK_FALSE(first.pending);
  CHECK(second.pending);
  CHECK_FALSE(third.pending);
  CHECK(second.priority == 1);

  std::array<std::int16_t, 160> transition{};
  engine.render(transition.data(), transition.size());
  CHECK_FALSE(engine.voiceInfo(1).pending);
  CHECK(engine.voiceInfo(1).priority == 3);
  CHECK(largestAdjacentJump(transition.data(), transition.size()) < 24000);
}

TEST_CASE("voice steal release keeps the outgoing tone gain") {
  cadenza::audio::AudioEngine engine;
  auto outgoing = tone(cadenza::audio::Waveform::Triangle, 441.0F, 0.2F, 0);
  outgoing.gain = 0.1F;
  auto silent = tone(cadenza::audio::Waveform::Triangle, 500.0F, 0.2F, 1);
  silent.gain = 0.0F;
  REQUIRE(engine.play(outgoing));
  REQUIRE(engine.play(silent));
  REQUIRE(engine.play(silent));
  std::array<std::int16_t, 25> warmup{};
  engine.render(warmup.data(), warmup.size());

  auto incoming = tone(cadenza::audio::Waveform::Triangle, 700.0F, 0.1F, 3);
  incoming.gain = 1.0F;
  REQUIRE(engine.play(incoming));
  std::array<std::int16_t, cadenza::audio::AudioEngine::kStealReleaseSamples>
      release{};
  engine.render(release.data(), release.size());

  // The final outgoing sample is scaled by 1/64 and the old 0.1 gain. The
  // pending tone's gain must not leak backward into that sample.
  CHECK(std::abs(static_cast<int>(release.back())) < 100);
}
