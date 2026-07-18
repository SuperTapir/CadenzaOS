#include "cadenza/desktop/sdl_audio_output.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace cadenza::desktop {

bool SdlAudioOutput::start(
    audio::InteractionSoundService& service) noexcept {
  if (stream_) return true;
  SDL_AudioSpec spec{};
  spec.format = SDL_AUDIO_S16;
  spec.channels = 1;
  spec.freq = static_cast<int>(audio::kSampleRate);
  service_ = &service;
  stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                      &SdlAudioOutput::feed, this);
  if (!stream_) {
    service_ = nullptr;
    return false;
  }
  if (!SDL_ResumeAudioStreamDevice(stream_)) {
    SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
    service_ = nullptr;
    return false;
  }
  return true;
}

void SdlAudioOutput::stop() noexcept {
  if (!stream_) return;
  SDL_PauseAudioStreamDevice(stream_);
  if (SDL_LockAudioStream(stream_)) {
    service_ = nullptr;
    SDL_UnlockAudioStream(stream_);
  }
  SDL_DestroyAudioStream(stream_);
  stream_ = nullptr;
  service_ = nullptr;
}

void SDLCALL SdlAudioOutput::feed(void* userdata, SDL_AudioStream* stream,
                                  int additionalAmount,
                                  int) noexcept {
  auto* output = static_cast<SdlAudioOutput*>(userdata);
  if (!output || !output->service_ || additionalAmount <= 0) return;
  output->callbackCount_.fetch_add(1, std::memory_order_relaxed);
  std::array<std::int16_t, 256> samples{};
  int remainingSamples =
      (additionalAmount + static_cast<int>(sizeof(std::int16_t)) - 1) /
      static_cast<int>(sizeof(std::int16_t));
  while (remainingSamples > 0) {
    const std::size_t count = std::min<std::size_t>(
        samples.size(), static_cast<std::size_t>(remainingSamples));
    output->service_->render(samples.data(), count);
    if (!SDL_PutAudioStreamData(
            stream, samples.data(),
            static_cast<int>(count * sizeof(std::int16_t)))) {
      return;
    }
    remainingSamples -= static_cast<int>(count);
  }
}

}  // namespace cadenza::desktop
