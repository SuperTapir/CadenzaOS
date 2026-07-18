#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/audio/audio_types.h"

namespace cadenza::audio {

enum class Waveform : std::uint8_t { Triangle, Square, Noise };

struct ToneSpec {
  Waveform waveform = Waveform::Triangle;
  float startFrequencyHz = 440.0F;
  float endFrequencyHz = 440.0F;
  float attackSeconds = 0.002F;
  float durationSeconds = 0.05F;
  float releaseSeconds = 0.005F;
  float gain = 0.25F;
  std::uint8_t priority = 1;
};

struct VoiceInfo {
  bool active = false;
  bool pending = false;
  std::uint8_t priority = 0;
  std::uint64_t sequence = 0;
};

class AudioEngine {
 public:
  static constexpr std::size_t kVoiceCount = 3;
  static constexpr std::uint32_t kStealReleaseSamples = 64;

  explicit AudioEngine(std::uint32_t noiseSeed = 0xCADA1234U) noexcept
      : noiseSeed_(noiseSeed ? noiseSeed : 1U) {}

  bool play(const ToneSpec& spec) noexcept;
  void stopAll() noexcept;
  void setMasterGain(float gain) noexcept;
  void render(std::int16_t* samples, std::size_t count) noexcept;

  std::size_t activeVoiceCount() const noexcept;
  VoiceInfo voiceInfo(std::size_t index) const noexcept;

 private:
  struct PreparedTone {
    Waveform waveform = Waveform::Triangle;
    float startFrequencyHz = 440.0F;
    float endFrequencyHz = 440.0F;
    float gain = 0.25F;
    std::uint32_t totalSamples = 0;
    std::uint32_t attackSamples = 0;
    std::uint32_t releaseSamples = 0;
    std::uint8_t priority = 1;
  };

  struct Voice {
    PreparedTone tone;
    PreparedTone pendingTone;
    float phase = 0.0F;
    std::uint32_t ageSamples = 0;
    std::uint32_t noiseState = 1;
    std::uint32_t stealSamplesRemaining = 0;
    std::uint64_t sequence = 0;
    bool active = false;
    bool pending = false;
  };

  static bool prepare(const ToneSpec& input, PreparedTone& output) noexcept;
  void start(Voice& voice, const PreparedTone& tone) noexcept;
  float renderVoice(Voice& voice) noexcept;

  std::array<Voice, kVoiceCount> voices_{};
  float masterGain_ = 0.70F;
  std::uint32_t noiseSeed_ = 1;
  std::uint64_t nextSequence_ = 1;
};

}  // namespace cadenza::audio
