#include "cadenza/audio/sound_cue_library.h"

namespace cadenza::audio {
namespace {
ToneSpec tone(Waveform waveform, float startHz, float endHz, float duration,
              float gain, std::uint8_t priority, float attack = 0.002F,
              float release = 0.005F) noexcept {
  ToneSpec value;
  value.waveform = waveform;
  value.startFrequencyHz = startHz;
  value.endFrequencyHz = endHz;
  value.attackSeconds = attack;
  value.durationSeconds = duration;
  value.releaseSeconds = release;
  value.gain = gain;
  value.priority = priority;
  return value;
}

const std::array<SoundCueDefinition,
                 static_cast<std::size_t>(SoundCue::Count)>
    kPalette{{
        {{{tone(Waveform::Triangle, 850.0F, 900.0F, 0.034F, 0.26F, 0), {}}},
         1},
        {{{tone(Waveform::Triangle, 520.0F, 390.0F, 0.055F, 0.22F, 1), {}}},
         1},
        {{{tone(Waveform::Triangle, 680.0F, 930.0F, 0.105F, 0.28F, 2),
           tone(Waveform::Square, 880.0F, 1180.0F, 0.095F, 0.08F, 2)}},
         2},
        {{{tone(Waveform::Triangle, 940.0F, 660.0F, 0.110F, 0.27F, 2),
           tone(Waveform::Square, 760.0F, 540.0F, 0.095F, 0.07F, 2)}},
         2},
        {{{tone(Waveform::Triangle, 700.0F, 900.0F, 0.075F, 0.24F, 1),
           tone(Waveform::Square, 900.0F, 1080.0F, 0.060F, 0.05F, 1)}},
         2},
        {{{tone(Waveform::Triangle, 900.0F, 690.0F, 0.070F, 0.22F, 1),
           tone(Waveform::Square, 760.0F, 580.0F, 0.055F, 0.05F, 1)}},
         2},
        {{{tone(Waveform::Noise, 900.0F, 500.0F, 0.060F, 0.10F, 3, 0.001F,
                0.008F),
           tone(Waveform::Triangle, 560.0F, 330.0F, 0.080F, 0.20F, 3)}},
         2},
    }};
}  // namespace

const SoundCueDefinition& soundCueDefinition(SoundCue cue) noexcept {
  const std::size_t index = static_cast<std::size_t>(cue);
  return kPalette[index < kPalette.size() ? index : 0];
}

const char* soundCueName(SoundCue cue) noexcept {
  switch (cue) {
    case SoundCue::Navigate:
      return "navigate";
    case SoundCue::Boundary:
      return "boundary";
    case SoundCue::Confirm:
      return "confirm";
    case SoundCue::Back:
      return "back";
    case SoundCue::ToggleOn:
      return "toggle-on";
    case SoundCue::ToggleOff:
      return "toggle-off";
    case SoundCue::Reject:
      return "reject";
    case SoundCue::Count:
      return "invalid";
  }
  return "invalid";
}

}  // namespace cadenza::audio
