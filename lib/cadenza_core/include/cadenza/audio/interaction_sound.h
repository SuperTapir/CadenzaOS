#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>

#include "cadenza/audio/audio_command_queue.h"
#include "cadenza/audio/audio_engine.h"
#include "cadenza/core/core_types.h"

namespace cadenza::audio {

struct SoundCueProfile {
  float startFrequencyHz = 0.0F;
  float endFrequencyHz = 0.0F;
  float durationSeconds = 0.0F;
  std::uint8_t toneCount = 0;
};

constexpr SoundVolume nextSoundVolume(SoundVolume volume) noexcept {
  switch (volume) {
    case SoundVolume::Muted:
      return SoundVolume::Low;
    case SoundVolume::Low:
      return SoundVolume::Medium;
    case SoundVolume::Medium:
      return SoundVolume::High;
    case SoundVolume::High:
    case SoundVolume::Count:
      return SoundVolume::Muted;
  }
  return SoundVolume::Muted;
}

const char* soundVolumeName(SoundVolume volume) noexcept;

class InteractionSoundService {
 public:
  static constexpr Seconds kNavigateCooldownSeconds = 0.025F;

  InteractionSoundService() noexcept;

  bool play(SoundCue cue) noexcept;
  bool setVolume(SoundVolume volume) noexcept;
  void stopAll() noexcept;
  void advance(Seconds delta) noexcept;

  // Audio-consumer methods. Only one callback/task may call these.
  void render(std::int16_t* samples, std::size_t count) noexcept;
  std::size_t activeVoiceCount() const noexcept {
    return engine_.activeVoiceCount();
  }

  SoundVolume volume() const noexcept { return volume_; }
  SoundCue lastAcceptedCue() const noexcept { return lastAcceptedCue_; }
  bool hasAcceptedCue() const noexcept { return hasAcceptedCue_; }
  std::size_t pendingCommandCount() const noexcept {
    return commands_.sizeApprox();
  }
  std::uint32_t overflowCount() const noexcept {
    return commands_.overflowCount();
  }

  static SoundCueProfile profile(SoundCue cue) noexcept;

 private:
  void consumeCommands() noexcept;
  static float volumeGain(SoundVolume volume) noexcept;

  AudioCommandQueue commands_;
  AudioEngine engine_;
  SoundVolume volume_ = SoundVolume::Medium;
  SoundVolume consumerVolume_ = SoundVolume::Medium;
  SoundCue lastAcceptedCue_ = SoundCue::Navigate;
  Seconds navigateCooldown_ = 0.0F;
  bool hasAcceptedCue_ = false;
  // Out-of-band safety mailbox: a full SPSC cue queue must never delay silence.
  std::atomic<bool> muteRequested_{false};
  std::atomic<bool> stopRequested_{false};
};

}  // namespace cadenza::audio
