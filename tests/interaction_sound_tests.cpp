#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>

#include "cadenza/audio/interaction_sound.h"
#include "cadenza/audio/sound_cue_library.h"

namespace {
std::uint64_t pcmHash(const std::int16_t* samples, std::size_t count) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (std::size_t index = 0; index < count; ++index) {
    const auto value = static_cast<std::uint16_t>(samples[index]);
    hash = (hash ^ static_cast<std::uint8_t>(value & 0xFFU)) *
           1099511628211ULL;
    hash = (hash ^ static_cast<std::uint8_t>(value >> 8U)) *
           1099511628211ULL;
  }
  return hash;
}

std::int16_t peakFor(cadenza::audio::SoundVolume volume) {
  cadenza::audio::InteractionSoundService service;
  REQUIRE(service.setVolume(volume));
  REQUIRE(service.play(cadenza::audio::SoundCue::Confirm));
  std::array<std::int16_t, 4096> samples{};
  service.render(samples.data(), samples.size());
  std::int16_t peak = 0;
  for (const auto sample : samples) {
    peak = std::max<std::int16_t>(
        peak, static_cast<std::int16_t>(std::min(32767, std::abs(sample))));
  }
  return peak;
}
}  // namespace

TEST_CASE("sound language exposes stable directional cue profiles") {
  const auto navigate =
      cadenza::audio::InteractionSoundService::profile(
          cadenza::audio::SoundCue::Navigate);
  const auto confirm = cadenza::audio::InteractionSoundService::profile(
      cadenza::audio::SoundCue::Confirm);
  const auto back = cadenza::audio::InteractionSoundService::profile(
      cadenza::audio::SoundCue::Back);
  const auto toggleOn = cadenza::audio::InteractionSoundService::profile(
      cadenza::audio::SoundCue::ToggleOn);
  const auto toggleOff = cadenza::audio::InteractionSoundService::profile(
      cadenza::audio::SoundCue::ToggleOff);

  CHECK(navigate.durationSeconds <= 0.012F);
  CHECK(navigate.toneCount == 4);
  CHECK(confirm.startFrequencyHz < confirm.endFrequencyHz);
  CHECK(back.startFrequencyHz > back.endFrequencyHz);
  CHECK(toggleOn.startFrequencyHz < toggleOn.endFrequencyHz);
  CHECK(toggleOff.startFrequencyHz > toggleOff.endFrequencyHz);
  CHECK(confirm.durationSeconds < 0.30F);
  CHECK(back.durationSeconds < 0.26F);
}

TEST_CASE("confirm and back preserve approved two-strike calibration") {
  const auto& confirm = cadenza::audio::soundCueDefinition(
      cadenza::audio::SoundCue::Confirm);
  const auto& back = cadenza::audio::soundCueDefinition(
      cadenza::audio::SoundCue::Back);
  REQUIRE(confirm.count == 2);
  REQUIRE(back.count == 2);

  const auto& confirmFirst = confirm.events[0];
  const auto& confirmSecond = confirm.events[1];
  CHECK(confirmFirst.delaySeconds == doctest::Approx(0.0F));
  CHECK(confirmSecond.delaySeconds >= 0.055F);
  CHECK(confirmSecond.delaySeconds <= 0.070F);
  CHECK(confirmFirst.tone.startFrequencyHz == doctest::Approx(660.0F).epsilon(0.04));
  CHECK(std::abs(confirmFirst.tone.endFrequencyHz -
                 confirmFirst.tone.startFrequencyHz) <= 12.0F);
  CHECK(confirmSecond.tone.startFrequencyHz == doctest::Approx(990.0F).epsilon(0.04));
  CHECK(std::abs(confirmSecond.tone.endFrequencyHz -
                 confirmSecond.tone.startFrequencyHz) <= 16.0F);
  CHECK(confirmFirst.tone.durationSeconds >= 0.155F);
  CHECK(confirmSecond.tone.durationSeconds >= 0.170F);
  CHECK(confirmSecond.delaySeconds + confirmSecond.tone.durationSeconds ==
        doctest::Approx(0.245F).epsilon(0.02));

  const auto& backFirst = back.events[0];
  const auto& backSecond = back.events[1];
  CHECK(backFirst.delaySeconds == doctest::Approx(0.0F));
  CHECK(backSecond.delaySeconds >= 0.035F);
  CHECK(backSecond.delaySeconds <= 0.050F);
  CHECK(backFirst.tone.startFrequencyHz == doctest::Approx(990.0F).epsilon(0.04));
  CHECK(std::abs(backFirst.tone.endFrequencyHz -
                 backFirst.tone.startFrequencyHz) <= 16.0F);
  CHECK(backSecond.tone.startFrequencyHz == doctest::Approx(590.0F).epsilon(0.05));
  CHECK(std::abs(backSecond.tone.endFrequencyHz -
                 backSecond.tone.startFrequencyHz) <= 12.0F);
  CHECK(backFirst.tone.durationSeconds >= 0.120F);
  CHECK(backSecond.tone.durationSeconds >= 0.155F);
  CHECK(backSecond.delaySeconds + backSecond.tone.durationSeconds ==
        doctest::Approx(0.210F).epsilon(0.02));
}

TEST_CASE("Menu surfaces preserve approved low directional single-onset cues") {
  using namespace cadenza::audio;
  const auto& menuOpen = soundCueDefinition(SoundCue::MenuOpen);
  const auto& menuClose = soundCueDefinition(SoundCue::MenuClose);
  REQUIRE(menuOpen.count == 2);
  REQUIRE(menuClose.count == 2);

  for (const auto* definition : {&menuOpen, &menuClose}) {
    CHECK(definition->events[0].delaySeconds == doctest::Approx(0.0F));
    CHECK(definition->events[1].delaySeconds == doctest::Approx(0.0F));
    CHECK(definition->events[0].tone.waveform == Waveform::Sine);
    CHECK(definition->events[1].tone.waveform == Waveform::Noise);
    CHECK(definition->events[0].tone.envelopeCurve ==
          EnvelopeCurve::Exponential);
    CHECK(definition->events[1].tone.envelopeCurve ==
          EnvelopeCurve::Exponential);
  }

  const auto openProfile = InteractionSoundService::profile(SoundCue::MenuOpen);
  const auto closeProfile =
      InteractionSoundService::profile(SoundCue::MenuClose);
  CHECK(openProfile.startFrequencyHz == doctest::Approx(340.0F));
  CHECK(openProfile.endFrequencyHz == doctest::Approx(680.0F));
  CHECK(openProfile.durationSeconds == doctest::Approx(0.160F));
  CHECK(closeProfile.startFrequencyHz == doctest::Approx(640.0F));
  CHECK(closeProfile.endFrequencyHz == doctest::Approx(290.0F));
  CHECK(closeProfile.durationSeconds == doctest::Approx(0.140F));
  CHECK(openProfile.startFrequencyHz < openProfile.endFrequencyHz);
  CHECK(closeProfile.startFrequencyHz > closeProfile.endFrequencyHz);
}

TEST_CASE("semantic hierarchy exposes stable cues plus Timer and Menu surfaces") {
  constexpr std::array<const char*, 18> expected{{
      "navigate",     "boundary",     "confirm",    "back",
      "toggle-on",    "toggle-off",   "reject",     "complete",
      "warning",      "failure",      "notification", "connect",
      "disconnect",   "power-on",     "power-off", "timer-complete",
      "menu-open",    "menu-close",
  }};
  CHECK(static_cast<std::size_t>(cadenza::audio::SoundCue::Count) ==
        expected.size());
  for (std::size_t index = 0; index < expected.size(); ++index) {
    CAPTURE(index);
    CHECK(std::string(cadenza::audio::soundCueName(
              static_cast<cadenza::audio::SoundCue>(index))) == expected[index]);
  }
}

TEST_CASE("every authored cue has bounded nonzero edge ramps") {
  for (std::size_t cueIndex = 0;
       cueIndex < static_cast<std::size_t>(cadenza::audio::SoundCue::Count);
       ++cueIndex) {
    const auto cue = static_cast<cadenza::audio::SoundCue>(cueIndex);
    const auto& definition = cadenza::audio::soundCueDefinition(cue);
    CAPTURE(cadenza::audio::soundCueName(cue));
    REQUIRE(definition.count > 0);
    REQUIRE(definition.count <= cadenza::audio::kMaximumTonesPerCue);
    for (std::size_t toneIndex = 0; toneIndex < definition.count; ++toneIndex) {
      const auto& event = definition.events[toneIndex];
      const auto& tone = event.tone;
      CHECK(event.delaySeconds >= 0.0F);
      CHECK(tone.attackSeconds > 0.0F);
      CHECK(tone.releaseSeconds > 0.0F);
      CHECK(tone.durationSeconds <= 0.80F);
      CHECK(event.delaySeconds + tone.durationSeconds <= 0.82F);
    }
  }

  const auto boundary = cadenza::audio::InteractionSoundService::profile(
      cadenza::audio::SoundCue::Boundary);
  const auto reject = cadenza::audio::InteractionSoundService::profile(
      cadenza::audio::SoundCue::Reject);
  CHECK(boundary.toneCount != reject.toneCount);
  CHECK(boundary.durationSeconds != reject.durationSeconds);
}

TEST_CASE("Timer Complete is a distinct bounded three-strike bell chord") {
  const auto timerComplete = cadenza::audio::InteractionSoundService::profile(
      cadenza::audio::SoundCue::TimerComplete);
  const auto complete = cadenza::audio::InteractionSoundService::profile(
      cadenza::audio::SoundCue::Complete);
  CHECK(timerComplete.toneCount == 4);
  CHECK(timerComplete.durationSeconds > complete.durationSeconds);
  CHECK(timerComplete.durationSeconds <= 0.82F);
}

TEST_CASE("delayed cue events start on schedule and are cleared by stop") {
  cadenza::audio::InteractionSoundService service;
  REQUIRE(service.play(cadenza::audio::SoundCue::Confirm));
  std::array<std::int16_t, 64> samples{};
  service.render(samples.data(), samples.size());
  CHECK(service.scheduledEventCount() > 0);
  service.stopAll();
  samples.fill(1234);
  service.render(samples.data(), samples.size());
  CHECK(service.scheduledEventCount() == 0);
  CHECK(service.activeVoiceCount() == 0);
  CHECK(std::all_of(samples.begin(), samples.end(),
                    [](std::int16_t sample) { return sample == 0; }));
}

TEST_CASE("all semantic hierarchy cues render deterministically and end silent") {
  constexpr std::size_t kSamples = 44100;
  constexpr std::array<std::uint64_t, 18> kGolden{{
      0x42ECB646DBC2F138ULL, 0x4DA394CE9C2148C2ULL,
      0x718BD5DCD3F36003ULL, 0x1AA4B6B726F7D759ULL,
      0x093E015B6B49C061ULL, 0xE184E7C5DF4F77CFULL,
      0x5B1F379B57A0775CULL, 0xA81782D63B7A0F48ULL,
      0x13E57E34362B7C70ULL, 0xF85F91ED8053C0ACULL,
      0x1E33454BC817A8F4ULL, 0x544F48552963AB96ULL,
      0xE18B997BCB79D023ULL, 0xCAB9268E54DCD0EEULL,
      0xFF77B74687A40111ULL, 0xF5AB40E42CF75F78ULL,
      0x7E65C35377921CA6ULL, 0x68D82E874AB181F1ULL,
  }};
  for (std::size_t cueIndex = 0;
       cueIndex < static_cast<std::size_t>(cadenza::audio::SoundCue::Count);
       ++cueIndex) {
    const auto cue = static_cast<cadenza::audio::SoundCue>(cueIndex);
    cadenza::audio::InteractionSoundService first;
    cadenza::audio::InteractionSoundService second;
    REQUIRE(first.play(cue));
    REQUIRE(second.play(cue));
    std::array<std::int16_t, kSamples> a{};
    std::array<std::int16_t, kSamples> b{};
    first.render(a.data(), a.size());
    second.render(b.data(), b.size());
    CAPTURE(cadenza::audio::soundCueName(cue));
    CHECK(a == b);
    CHECK(pcmHash(a.data(), a.size()) == kGolden[cueIndex]);
    CHECK(std::any_of(a.begin(), a.end(),
                      [](std::int16_t sample) { return sample != 0; }));
    CHECK(std::all_of(a.end() - 512, a.end(),
                      [](std::int16_t sample) { return sample == 0; }));
    CHECK(first.activeVoiceCount() == 0);
    CHECK(first.scheduledEventCount() == 0);
  }
}

TEST_CASE("navigate cooldown drops repeats without delayed playback") {
  cadenza::audio::InteractionSoundService service;
  REQUIRE(service.play(cadenza::audio::SoundCue::Navigate));
  CHECK_FALSE(service.play(cadenza::audio::SoundCue::Navigate));
  CHECK(service.pendingCommandCount() == 1);
  service.advance(0.03F);
  REQUIRE(service.play(cadenza::audio::SoundCue::Navigate));
  CHECK(service.pendingCommandCount() == 2);

  std::array<std::int16_t, 4096> samples{};
  service.render(samples.data(), samples.size());
  CHECK(service.pendingCommandCount() == 0);
  CHECK(std::count_if(samples.begin(), samples.end(),
                      [](std::int16_t sample) { return sample != 0; }) > 0);
  service.render(samples.data(), samples.size());
  CHECK(std::all_of(samples.begin(), samples.end(),
                    [](std::int16_t sample) { return sample == 0; }));
}

TEST_CASE("muting immediately clears active and queued sound") {
  cadenza::audio::InteractionSoundService service;
  REQUIRE(service.play(cadenza::audio::SoundCue::Confirm));
  std::array<std::int16_t, 128> samples{};
  service.render(samples.data(), samples.size());
  CHECK(std::any_of(samples.begin(), samples.end(),
                    [](std::int16_t sample) { return sample != 0; }));

  REQUIRE(service.setVolume(cadenza::audio::SoundVolume::Muted));
  samples.fill(1234);
  service.render(samples.data(), samples.size());
  CHECK(std::all_of(samples.begin(), samples.end(),
                    [](std::int16_t sample) { return sample == 0; }));
  CHECK(service.activeVoiceCount() == 0);
  CHECK(service.scheduledEventCount() == 0);
  CHECK_FALSE(service.play(cadenza::audio::SoundCue::Reject));
}

TEST_CASE("muting cannot be blocked by a saturated critical queue") {
  cadenza::audio::InteractionSoundService service;
  for (std::size_t index = 0;
       index < cadenza::audio::AudioCommandQueue::kCapacity; ++index) {
    REQUIRE(service.play(cadenza::audio::SoundCue::Confirm));
  }
  CHECK(service.pendingCommandCount() ==
        cadenza::audio::AudioCommandQueue::kCapacity);

  REQUIRE(service.setVolume(cadenza::audio::SoundVolume::Muted));
  std::array<std::int16_t, 256> samples{};
  samples.fill(1234);
  service.render(samples.data(), samples.size());
  CHECK(std::all_of(samples.begin(), samples.end(),
                    [](std::int16_t sample) { return sample == 0; }));
  CHECK(service.activeVoiceCount() == 0);
}

TEST_CASE("volume levels are ordered and cycle through all session values") {
  const auto low = peakFor(cadenza::audio::SoundVolume::Low);
  const auto medium = peakFor(cadenza::audio::SoundVolume::Medium);
  const auto high = peakFor(cadenza::audio::SoundVolume::High);
  CHECK(low > 0);
  CHECK(low < medium);
  CHECK(medium < high);

  using cadenza::audio::SoundVolume;
  CHECK(cadenza::audio::nextSoundVolume(SoundVolume::Medium) ==
        SoundVolume::High);
  CHECK(cadenza::audio::nextSoundVolume(SoundVolume::High) ==
        SoundVolume::Muted);
  CHECK(cadenza::audio::nextSoundVolume(SoundVolume::Muted) ==
        SoundVolume::Low);
  CHECK(cadenza::audio::nextSoundVolume(SoundVolume::Low) ==
        SoundVolume::Medium);
}
