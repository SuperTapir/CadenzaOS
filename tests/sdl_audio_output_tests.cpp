#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <array>
#include <cstdint>

#include "cadenza/audio/interaction_sound.h"
#include "cadenza/desktop/sdl_audio_output.h"

TEST_CASE("SDL audio failure leaves the portable sound service usable") {
  SDL_Quit();
  cadenza::audio::InteractionSoundService service;
  cadenza::desktop::SdlAudioOutput output;
  CHECK_FALSE(output.start(service));
  CHECK_FALSE(output.active());

  REQUIRE(service.play(cadenza::audio::SoundCue::Confirm));
  std::array<std::int16_t, 512> samples{};
  service.render(samples.data(), samples.size());
  CHECK(std::any_of(samples.begin(), samples.end(),
                    [](std::int16_t value) { return value != 0; }));
}

TEST_CASE("SDL dummy callback consumes audio independently of App updates") {
  REQUIRE(SDL_Init(SDL_INIT_AUDIO));
  cadenza::audio::InteractionSoundService service;
  cadenza::desktop::SdlAudioOutput output;
  REQUIRE(output.start(service));
  REQUIRE(output.active());
  REQUIRE(service.play(cadenza::audio::SoundCue::Confirm));

  const std::uint64_t deadline = SDL_GetTicks() + 500U;
  while (output.callbackCount() == 0 && SDL_GetTicks() < deadline) {
    SDL_Delay(1);
  }
  CHECK(output.callbackCount() > 0);
  CHECK(service.pendingCommandCount() == 0);

  // No App/simulation update occurs here. The device callback owns real audio
  // time and must still finish the existing cue naturally.
  const auto cueDuration =
      cadenza::audio::InteractionSoundService::profile(
          cadenza::audio::SoundCue::Confirm)
          .durationSeconds;
  SDL_Delay(static_cast<std::uint32_t>(cueDuration * 1000.0F) + 100U);

  output.stop();
  CHECK_FALSE(output.active());
  CHECK(service.activeVoiceCount() == 0);
  const auto callbacksAfterStop = output.callbackCount();
  SDL_Delay(10);
  CHECK(output.callbackCount() == callbacksAfterStop);
  SDL_Quit();
}
