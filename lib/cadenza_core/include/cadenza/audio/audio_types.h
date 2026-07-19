#pragma once

#include <array>
#include <cstddef>
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
  MenuOpen,
  MenuClose,
  Count,
};

enum class SoundVolume : std::uint8_t { Muted, Low, Medium, High, Count };

struct MusicalNoteSet {
  static constexpr std::size_t kCapacity = 4;
  static constexpr std::uint8_t kMinimumMidiNote = 21;
  static constexpr std::uint8_t kMaximumMidiNote = 108;

  std::array<std::uint8_t, kCapacity> notes{};
  std::uint8_t count = 0;

  constexpr bool valid() const noexcept {
    if (count == 0 || count > kCapacity) return false;
    for (std::size_t index = 0; index < count; ++index) {
      if (notes[index] < kMinimumMidiNote ||
          notes[index] > kMaximumMidiNote) return false;
    }
    return true;
  }

  static constexpr MusicalNoteSet single(std::uint8_t note) noexcept {
    MusicalNoteSet result;
    result.notes[0] = note;
    result.count = 1;
    return result;
  }
};

enum class AudioCommandKind : std::uint8_t {
  PlayCue,
  PlayNotes,
  SetVolume,
  StopAll,
};

struct AudioCommand {
  AudioCommandKind kind = AudioCommandKind::StopAll;
  SoundCue cue = SoundCue::Navigate;
  SoundVolume volume = SoundVolume::Medium;
  MusicalNoteSet noteSet{};

  static constexpr AudioCommand play(SoundCue value) noexcept {
    AudioCommand command;
    command.kind = AudioCommandKind::PlayCue;
    command.cue = value;
    return command;
  }

  static constexpr AudioCommand playNotes(MusicalNoteSet value) noexcept {
    AudioCommand command;
    command.kind = AudioCommandKind::PlayNotes;
    command.noteSet = value;
    return command;
  }

  static constexpr AudioCommand setVolume(SoundVolume value) noexcept {
    AudioCommand command;
    command.kind = AudioCommandKind::SetVolume;
    command.volume = value;
    return command;
  }

  static constexpr AudioCommand stopAll() noexcept { return {}; }

  constexpr bool droppable() const noexcept {
    return kind == AudioCommandKind::PlayNotes ||
           (kind == AudioCommandKind::PlayCue &&
            (cue == SoundCue::Navigate || cue == SoundCue::Boundary));
  }
};

}  // namespace cadenza::audio
