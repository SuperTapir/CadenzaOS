#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cstdint>

#include "cadenza/audio/interaction_sound.h"
#include "cadenza/audio/sound_cue_library.h"

namespace {
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

  CHECK(navigate.durationSeconds <= 0.04F);
  CHECK(navigate.toneCount == 1);
  CHECK(confirm.startFrequencyHz < confirm.endFrequencyHz);
  CHECK(back.startFrequencyHz > back.endFrequencyHz);
  CHECK(toggleOn.startFrequencyHz < toggleOn.endFrequencyHz);
  CHECK(toggleOff.startFrequencyHz > toggleOff.endFrequencyHz);
  CHECK(confirm.durationSeconds < 0.15F);
  CHECK(back.durationSeconds < 0.15F);
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
      const auto& tone = definition.tones[toneIndex];
      CHECK(tone.attackSeconds > 0.0F);
      CHECK(tone.releaseSeconds > 0.0F);
      CHECK(tone.durationSeconds <= 0.18F);
    }
  }

  const auto boundary = cadenza::audio::InteractionSoundService::profile(
      cadenza::audio::SoundCue::Boundary);
  const auto reject = cadenza::audio::InteractionSoundService::profile(
      cadenza::audio::SoundCue::Reject);
  CHECK(boundary.toneCount != reject.toneCount);
  CHECK(boundary.durationSeconds != reject.durationSeconds);
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
