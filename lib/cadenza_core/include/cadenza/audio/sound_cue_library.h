#pragma once

#include <array>
#include <cstdint>

#include "cadenza/audio/audio_engine.h"
#include "cadenza/audio/audio_types.h"

namespace cadenza::audio {

constexpr std::size_t kMaximumTonesPerCue = 4;

struct SoundEvent {
  ToneSpec tone{};
  float delaySeconds = 0.0F;
};

struct SoundCueDefinition {
  std::array<SoundEvent, kMaximumTonesPerCue> events{};
  std::uint8_t count = 0;
};

// This is the single authored sound palette. Runtime code consumes semantic
// SoundCue values and never depends on the synthesis parameters directly.
const SoundCueDefinition& soundCueDefinition(SoundCue cue) noexcept;
const char* soundCueName(SoundCue cue) noexcept;

}  // namespace cadenza::audio
