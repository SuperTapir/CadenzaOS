#pragma once

#include <cstdint>

namespace cadenza::audio {

constexpr float kSampleRate = 44100.0F;

enum class SoundCue : std::uint8_t {
  Navigate,
  Boundary,
  Confirm,
  Back,
  ToggleOn,
  ToggleOff,
  Reject,
  Complete,
  Warning,
  Failure,
  Notification,
  Connect,
  Disconnect,
  PowerOn,
  PowerOff,
  TimerComplete,
  Count,
};

enum class SoundVolume : std::uint8_t { Muted, Low, Medium, High, Count };

enum class AudioCommandKind : std::uint8_t { PlayCue, SetVolume, StopAll };

struct AudioCommand {
  AudioCommandKind kind = AudioCommandKind::StopAll;
  SoundCue cue = SoundCue::Navigate;
  SoundVolume volume = SoundVolume::Medium;

  static constexpr AudioCommand play(SoundCue value) noexcept {
    return {AudioCommandKind::PlayCue, value, SoundVolume::Medium};
  }

  static constexpr AudioCommand setVolume(SoundVolume value) noexcept {
    return {AudioCommandKind::SetVolume, SoundCue::Navigate, value};
  }

  static constexpr AudioCommand stopAll() noexcept { return {}; }

  constexpr bool droppable() const noexcept {
    return kind == AudioCommandKind::PlayCue &&
           (cue == SoundCue::Navigate || cue == SoundCue::Boundary);
  }
};

}  // namespace cadenza::audio
