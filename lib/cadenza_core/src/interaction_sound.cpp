#include "cadenza/audio/interaction_sound.h"

#include <algorithm>
#include <cmath>

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

bool InteractionSoundService::playNotes(const MusicalNoteSet& notes) noexcept {
  if (!notes.valid() || volume_ == SoundVolume::Muted) return false;
  return commands_.tryPush(AudioCommand::playNotes(notes));
}

float InteractionSoundService::midiFrequency(std::uint8_t note) noexcept {
  return 440.0F * std::pow(2.0F, (static_cast<float>(note) - 69.0F) / 12.0F);
}

void InteractionSoundService::startNotes(
    const MusicalNoteSet& notes) noexcept {
  engine_.stopAll();
  clearScheduled();
  const float voiceGain = 0.42F / std::sqrt(static_cast<float>(notes.count));
  for (std::size_t index = 0; index < notes.count; ++index) {
    ToneSpec tone;
    tone.waveform = Waveform::Sine;
    tone.startFrequencyHz = midiFrequency(notes.notes[index]);
    tone.endFrequencyHz = tone.startFrequencyHz;
    tone.attackSeconds = 0.008F;
    tone.decaySeconds = 0.20F;
    tone.durationSeconds = 0.70F;
    tone.releaseSeconds = 0.08F;
    tone.gain = voiceGain;
    tone.secondHarmonicGain = 0.18F;
    tone.secondHarmonicPhaseCycles = 0.25F;
    tone.priority = 2;
    tone.envelopeCurve = EnvelopeCurve::Exponential;
    engine_.play(tone);
  }
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

bool InteractionSoundService::schedule(const SoundEvent& event) noexcept {
  if (!std::isfinite(event.delaySeconds) || event.delaySeconds < 0.0F) {
    return false;
  }
  if (event.delaySeconds <= 0.0F) return engine_.play(event.tone);
  for (auto& scheduled : scheduled_) {
    if (!scheduled.active) {
      scheduled.tone = event.tone;
      scheduled.remainingSamples = static_cast<std::uint32_t>(
          event.delaySeconds * kSampleRate + 0.5F);
      scheduled.active = true;
      return true;
    }
  }
  return false;
}

void InteractionSoundService::clearScheduled() noexcept {
  for (auto& event : scheduled_) event = {};
}

void InteractionSoundService::startDueEvents() noexcept {
  for (auto& event : scheduled_) {
    if (event.active && event.remainingSamples == 0U) {
      engine_.play(event.tone);
      event = {};
    }
  }
}

std::size_t InteractionSoundService::scheduledEventCount() const noexcept {
  return static_cast<std::size_t>(std::count_if(
      scheduled_.begin(), scheduled_.end(),
      [](const ScheduledEvent& event) { return event.active; }));
}

void InteractionSoundService::consumeCommands() noexcept {
  AudioCommand command;
  while (commands_.tryPop(command)) {
    switch (command.kind) {
      case AudioCommandKind::PlayCue: {
        if (consumerVolume_ == SoundVolume::Muted) break;
        const SoundCueDefinition& cue = soundCueDefinition(command.cue);
        for (std::uint8_t index = 0; index < cue.count; ++index) {
          schedule(cue.events[index]);
        }
        break;
      }
      case AudioCommandKind::PlayNotes:
        if (consumerVolume_ != SoundVolume::Muted && command.noteSet.valid()) {
          startNotes(command.noteSet);
        }
        break;
      case AudioCommandKind::SetVolume:
        consumerVolume_ = command.volume;
        engine_.setMasterGain(volumeGain(consumerVolume_));
        if (consumerVolume_ == SoundVolume::Muted) {
          engine_.stopAll();
          clearScheduled();
        }
        break;
      case AudioCommandKind::StopAll:
        engine_.stopAll();
        clearScheduled();
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
    clearScheduled();
  }
  if (stopRequested_.exchange(false, std::memory_order_acq_rel)) {
    engine_.stopAll();
    clearScheduled();
  }

  std::size_t rendered = 0;
  while (rendered < count) {
    startDueEvents();
    std::size_t chunk = count - rendered;
    for (const auto& event : scheduled_) {
      if (event.active) {
        chunk = std::min<std::size_t>(chunk, event.remainingSamples);
      }
    }
    if (chunk == 0U) continue;
    engine_.render(samples + rendered, chunk);
    for (auto& event : scheduled_) {
      if (event.active) {
        event.remainingSamples -= static_cast<std::uint32_t>(chunk);
      }
    }
    rendered += chunk;
  }
}

SoundCueProfile InteractionSoundService::profile(SoundCue cue) noexcept {
  const SoundCueDefinition& value = soundCueDefinition(cue);
  if (value.count == 0U) return {};
  std::size_t last = 0;
  float endSeconds = value.events[0].delaySeconds +
                     value.events[0].tone.durationSeconds;
  for (std::size_t index = 1; index < value.count; ++index) {
    const float candidateEnd = value.events[index].delaySeconds +
                               value.events[index].tone.durationSeconds;
    if (candidateEnd > endSeconds) {
      endSeconds = candidateEnd;
      last = index;
    }
  }
  // The event that resolves the cue carries its semantic direction. Earlier
  // events may be shared mechanical contacts (notably Toggle On/Off).
  float startFrequencyHz = value.events[last].tone.startFrequencyHz;
  const float endFrequencyHz = value.events[last].tone.endFrequencyHz;
  // Fixed-pitch multi-strike cues express direction across events rather than
  // by sweeping the resolving strike (Confirm 659 -> 988, Back 988 -> 587).
  if (value.count > 1U &&
      std::abs(endFrequencyHz - startFrequencyHz) < 1.0F) {
    startFrequencyHz = value.events[0].tone.startFrequencyHz;
  }
  return {startFrequencyHz, endFrequencyHz, endSeconds, value.count};
}

}  // namespace cadenza::audio
