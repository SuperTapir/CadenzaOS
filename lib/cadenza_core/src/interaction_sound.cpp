#include "cadenza/audio/interaction_sound.h"

#include <algorithm>

#include "cadenza/audio/sound_cue_library.h"

namespace cadenza::audio {

const char* soundVolumeName(SoundVolume volume) noexcept {
  switch (volume) {
    case SoundVolume::Muted:
      return "MUTED";
    case SoundVolume::Low:
      return "LOW";
    case SoundVolume::Medium:
      return "MEDIUM";
    case SoundVolume::High:
      return "HIGH";
    case SoundVolume::Count:
      return "INVALID";
  }
  return "INVALID";
}

InteractionSoundService::InteractionSoundService() noexcept {
  engine_.setMasterGain(volumeGain(consumerVolume_));
}

bool InteractionSoundService::play(SoundCue cue) noexcept {
  if (static_cast<std::size_t>(cue) >=
          static_cast<std::size_t>(SoundCue::Count) ||
      volume_ == SoundVolume::Muted ||
      (cue == SoundCue::Navigate && navigateCooldown_ > 0.0F)) {
    return false;
  }
  if (!commands_.tryPush(AudioCommand::play(cue))) return false;
  if (cue == SoundCue::Navigate) {
    navigateCooldown_ = kNavigateCooldownSeconds;
  }
  lastAcceptedCue_ = cue;
  hasAcceptedCue_ = true;
  return true;
}

bool InteractionSoundService::setVolume(SoundVolume volume) noexcept {
  if (volume >= SoundVolume::Count) return false;
  if (volume == volume_) return true;
  if (volume == SoundVolume::Muted) {
    // Silence is a safety/control operation, not a best-effort UI cue. Publish
    // it even when the normal command queue is saturated; render() applies it
    // after draining older commands so none can revive queued sound.
    muteRequested_.store(true, std::memory_order_release);
    commands_.tryPush(AudioCommand::setVolume(volume));
    volume_ = volume;
    return true;
  }
  if (!commands_.tryPush(AudioCommand::setVolume(volume))) return false;
  volume_ = volume;
  muteRequested_.store(false, std::memory_order_release);
  return true;
}

void InteractionSoundService::stopAll() noexcept {
  stopRequested_.store(true, std::memory_order_release);
  commands_.tryPush(AudioCommand::stopAll());
}

void InteractionSoundService::advance(Seconds delta) noexcept {
  navigateCooldown_ =
      std::max(0.0F, navigateCooldown_ - std::max(0.0F, delta));
}

float InteractionSoundService::volumeGain(SoundVolume volume) noexcept {
  switch (volume) {
    case SoundVolume::Muted:
      return 0.0F;
    case SoundVolume::Low:
      return 0.25F;
    case SoundVolume::Medium:
      return 0.45F;
    case SoundVolume::High:
      return 0.70F;
    case SoundVolume::Count:
      return 0.0F;
  }
  return 0.0F;
}

void InteractionSoundService::consumeCommands() noexcept {
  AudioCommand command;
  while (commands_.tryPop(command)) {
    switch (command.kind) {
      case AudioCommandKind::PlayCue: {
        if (consumerVolume_ == SoundVolume::Muted) break;
        const SoundCueDefinition& cue = soundCueDefinition(command.cue);
        for (std::uint8_t index = 0; index < cue.count; ++index) {
          engine_.play(cue.tones[index]);
        }
        break;
      }
      case AudioCommandKind::SetVolume:
        consumerVolume_ = command.volume;
        engine_.setMasterGain(volumeGain(consumerVolume_));
        if (consumerVolume_ == SoundVolume::Muted) engine_.stopAll();
        break;
      case AudioCommandKind::StopAll:
        engine_.stopAll();
        break;
    }
  }
}

void InteractionSoundService::render(std::int16_t* samples,
                                     std::size_t count) noexcept {
  consumeCommands();
  if (muteRequested_.exchange(false, std::memory_order_acq_rel)) {
    consumerVolume_ = SoundVolume::Muted;
    engine_.setMasterGain(0.0F);
    engine_.stopAll();
  }
  if (stopRequested_.exchange(false, std::memory_order_acq_rel)) {
    engine_.stopAll();
  }
  engine_.render(samples, count);
}

SoundCueProfile InteractionSoundService::profile(SoundCue cue) noexcept {
  const SoundCueDefinition& value = soundCueDefinition(cue);
  return {value.tones[0].startFrequencyHz, value.tones[0].endFrequencyHz,
          value.tones[0].durationSeconds, value.count};
}

}  // namespace cadenza::audio
