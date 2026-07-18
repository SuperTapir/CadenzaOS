#include "cadenza/audio/audio_engine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cadenza::audio {
namespace {
constexpr float kMinimumFrequencyHz = 20.0F;
constexpr float kMaximumFrequencyHz = 16000.0F;
constexpr float kMaximumDurationSeconds = 2.0F;
constexpr std::uint32_t kMinimumRampSamples = 8;
constexpr std::uint32_t kMinimumToneSamples = kMinimumRampSamples * 2;

float clamp01(float value) noexcept {
  return std::max(0.0F, std::min(1.0F, value));
}

float polyBlep(float phase, float phaseIncrement) noexcept {
  if (phase < phaseIncrement) {
    phase /= phaseIncrement;
    return phase + phase - phase * phase - 1.0F;
  }
  if (phase > 1.0F - phaseIncrement) {
    phase = (phase - 1.0F) / phaseIncrement;
    return phase * phase + phase + phase + 1.0F;
  }
  return 0.0F;
}

std::uint32_t secondsToSamples(float seconds) noexcept {
  return static_cast<std::uint32_t>(seconds * kSampleRate + 0.5F);
}
}  // namespace

bool AudioEngine::prepare(const ToneSpec& input,
                          PreparedTone& output) noexcept {
  if (!std::isfinite(input.startFrequencyHz) ||
      !std::isfinite(input.endFrequencyHz) ||
      !std::isfinite(input.attackSeconds) ||
      !std::isfinite(input.durationSeconds) ||
      !std::isfinite(input.releaseSeconds) || !std::isfinite(input.gain) ||
      input.startFrequencyHz <= 0.0F || input.endFrequencyHz <= 0.0F ||
      input.attackSeconds < 0.0F || input.releaseSeconds < 0.0F ||
      input.durationSeconds <= 0.0F) {
    return false;
  }

  output.waveform = input.waveform;
  output.startFrequencyHz = std::max(
      kMinimumFrequencyHz, std::min(kMaximumFrequencyHz, input.startFrequencyHz));
  output.endFrequencyHz = std::max(
      kMinimumFrequencyHz, std::min(kMaximumFrequencyHz, input.endFrequencyHz));
  output.gain = clamp01(input.gain);
  output.totalSamples = std::max(
      kMinimumToneSamples,
      secondsToSamples(std::min(kMaximumDurationSeconds, input.durationSeconds)));
  output.attackSamples =
      std::max(kMinimumRampSamples, secondsToSamples(input.attackSeconds));
  output.releaseSamples =
      std::max(kMinimumRampSamples, secondsToSamples(input.releaseSeconds));
  if (output.attackSamples + output.releaseSamples > output.totalSamples) {
    output.attackSamples = output.totalSamples / 2U;
    output.releaseSamples = output.totalSamples - output.attackSamples;
  }
  output.priority = input.priority;
  return true;
}

void AudioEngine::start(Voice& voice, const PreparedTone& tone) noexcept {
  voice.tone = tone;
  voice.phase = 0.0F;
  voice.ageSamples = 0;
  voice.noiseState = noiseSeed_ ^
                     static_cast<std::uint32_t>(nextSequence_) ^ 0x9E3779B9U;
  if (voice.noiseState == 0) voice.noiseState = 1;
  voice.stealSamplesRemaining = 0;
  voice.sequence = nextSequence_++;
  voice.active = true;
  voice.pending = false;
}

bool AudioEngine::play(const ToneSpec& spec) noexcept {
  PreparedTone prepared;
  if (!prepare(spec, prepared)) return false;

  for (auto& voice : voices_) {
    if (!voice.active) {
      start(voice, prepared);
      return true;
    }
  }

  std::size_t candidate = 0;
  for (std::size_t index = 1; index < voices_.size(); ++index) {
    const auto& current = voices_[index];
    const auto& selected = voices_[candidate];
    if (current.tone.priority < selected.tone.priority ||
        (current.tone.priority == selected.tone.priority &&
         current.sequence < selected.sequence)) {
      candidate = index;
    }
  }
  Voice& selected = voices_[candidate];
  if (prepared.priority < selected.tone.priority) return false;
  selected.pendingTone = prepared;
  selected.pending = true;
  selected.stealSamplesRemaining = kStealReleaseSamples;
  return true;
}

void AudioEngine::stopAll() noexcept {
  for (auto& voice : voices_) voice = {};
}

void AudioEngine::setMasterGain(float gain) noexcept {
  masterGain_ = std::isfinite(gain) ? clamp01(gain) : 0.0F;
}

float AudioEngine::renderVoice(Voice& voice) noexcept {
  if (!voice.active) return 0.0F;

  const PreparedTone& tone = voice.tone;
  const float life = tone.totalSamples > 1U
                         ? static_cast<float>(voice.ageSamples) /
                               static_cast<float>(tone.totalSamples - 1U)
                         : 1.0F;
  const float frequency = tone.startFrequencyHz +
                          (tone.endFrequencyHz - tone.startFrequencyHz) * life;
  const float phaseIncrement = frequency / kSampleRate;
  float wave = 0.0F;
  switch (tone.waveform) {
    case Waveform::Triangle:
      wave = 1.0F - 4.0F * std::abs(voice.phase - 0.5F);
      break;
    case Waveform::Square: {
      wave = voice.phase < 0.5F ? 1.0F : -1.0F;
      wave += polyBlep(voice.phase, phaseIncrement);
      float secondEdge = voice.phase + 0.5F;
      if (secondEdge >= 1.0F) secondEdge -= 1.0F;
      wave -= polyBlep(secondEdge, phaseIncrement);
      break;
    }
    case Waveform::Noise:
      voice.noiseState ^= voice.noiseState << 13U;
      voice.noiseState ^= voice.noiseState >> 17U;
      voice.noiseState ^= voice.noiseState << 5U;
      wave = static_cast<float>(static_cast<std::int32_t>(voice.noiseState)) /
             2147483648.0F;
      break;
  }

  float envelope = 1.0F;
  if (voice.ageSamples < tone.attackSamples) {
    envelope = static_cast<float>(voice.ageSamples) /
               static_cast<float>(tone.attackSamples);
  } else if (voice.ageSamples >= tone.totalSamples - tone.releaseSamples) {
    const std::uint32_t remaining =
        tone.totalSamples - 1U - voice.ageSamples;
    envelope = tone.releaseSamples > 1U
                   ? static_cast<float>(remaining) /
                         static_cast<float>(tone.releaseSamples - 1U)
                   : 0.0F;
  }
  if (voice.stealSamplesRemaining > 0U) {
    envelope *= static_cast<float>(voice.stealSamplesRemaining) /
                static_cast<float>(kStealReleaseSamples);
    --voice.stealSamplesRemaining;
  }

  voice.phase += phaseIncrement;
  if (voice.phase >= 1.0F) voice.phase -= 1.0F;
  ++voice.ageSamples;
  const float output = wave * envelope * tone.gain;

  if (voice.pending && voice.stealSamplesRemaining == 0U) {
    const PreparedTone pending = voice.pendingTone;
    start(voice, pending);
  } else if (voice.ageSamples >= tone.totalSamples) {
    voice = {};
  }
  return output;
}

void AudioEngine::render(std::int16_t* samples, std::size_t count) noexcept {
  if (!samples) return;
  for (std::size_t index = 0; index < count; ++index) {
    float mixed = 0.0F;
    for (auto& voice : voices_) mixed += renderVoice(voice);
    mixed = std::max(-1.0F, std::min(1.0F, mixed * masterGain_));
    const float scaled = mixed >= 0.0F ? mixed * 32767.0F : mixed * 32768.0F;
    const std::int32_t rounded =
        static_cast<std::int32_t>(scaled + (scaled >= 0.0F ? 0.5F : -0.5F));
    samples[index] = static_cast<std::int16_t>(rounded);
  }
}

std::size_t AudioEngine::activeVoiceCount() const noexcept {
  return static_cast<std::size_t>(std::count_if(
      voices_.begin(), voices_.end(), [](const Voice& voice) {
        return voice.active;
      }));
}

VoiceInfo AudioEngine::voiceInfo(std::size_t index) const noexcept {
  if (index >= voices_.size()) return {};
  const Voice& voice = voices_[index];
  return {voice.active, voice.pending, voice.tone.priority, voice.sequence};
}

}  // namespace cadenza::audio
