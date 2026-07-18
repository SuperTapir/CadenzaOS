#pragma once

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdint>

#include "cadenza/audio/interaction_sound.h"

namespace cadenza::desktop {

class SdlAudioOutput {
 public:
  SdlAudioOutput() noexcept = default;
  ~SdlAudioOutput() noexcept { stop(); }

  SdlAudioOutput(const SdlAudioOutput&) = delete;
  SdlAudioOutput& operator=(const SdlAudioOutput&) = delete;

  bool start(audio::InteractionSoundService& service) noexcept;
  void stop() noexcept;
  bool active() const noexcept { return stream_ != nullptr; }
  std::uint64_t callbackCount() const noexcept {
    return callbackCount_.load(std::memory_order_relaxed);
  }

 private:
  static void SDLCALL feed(void* userdata, SDL_AudioStream* stream,
                           int additionalAmount, int totalAmount) noexcept;

  SDL_AudioStream* stream_ = nullptr;
  audio::InteractionSoundService* service_ = nullptr;
  std::atomic<std::uint64_t> callbackCount_{0};
};

}  // namespace cadenza::desktop
