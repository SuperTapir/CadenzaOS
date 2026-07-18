#include "cadenza/audio/audio_engine.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cadenza::audio {
namespace {
constexpr float kMinimumFrequencyHz = 20.0F;
constexpr float kMaximumFrequencyHz = 16000.0F;
constexpr float kMaximumDurationSeconds = 2.0F;
constexpr std::uint32_t kMinimumRampSamples = 1;
constexpr std::uint32_t kMinimumToneSamples = kMinimumRampSamples * 2;

constexpr std::array<std::int16_t, 65> kQuarterSine{{
    0,     804,   1608,  2410,  3212,  4011,  4808,  5602,  6393,  7179,
    7962,  8739,  9512,  10278, 11039, 11793, 12539, 13279, 14010, 14732,
    15446, 16151, 16846, 17530, 18204, 18868, 19519, 20159, 20787, 21403,
    22005, 22594, 23170, 23731, 24279, 24811, 25329, 25832, 26319, 26790,
    27245, 27683, 28105, 28510, 28898, 29268, 29621, 29956, 30273, 30571,
    30852, 31113, 31356, 31580, 31785, 31971, 32137, 32285, 32412, 32521,
    32609, 32678, 32728, 32757, 32767,
}};

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

float sineSample(std::uint32_t index) noexcept {
  index &= 255U;
  const std::uint32_t quadrant = index >> 6U;
  const std::uint32_t offset = index & 63U;
  std::int32_t value = 0;
  switch (quadrant) {
    case 0:
      value = kQuarterSine[offset];
      break;
    case 1:
      value = kQuarterSine[64U - offset];
      break;
    case 2:
      value = -kQuarterSine[offset];
      break;
    default:
      value = -kQuarterSine[64U - offset];
      break;
  }
  return static_cast<float>(value) / 32767.0F;
}

float wavetableSine(float phase) noexcept {
  const float position = phase * 256.0F;
  const auto index = static_cast<std::uint32_t>(position);
  const float fraction = position - static_cast<float>(index);
  const float a = sineSample(index);
  const float b = sineSample(index + 1U);
  return a + (b - a) * fraction;
}
}  // namespace

bool AudioEngine::prepare(const ToneSpec& input,
                          PreparedTone& output) noexcept {
  if (!std::isfinite(input.startFrequencyHz) ||
      !std::isfinite(input.endFrequencyHz) ||
      !std::isfinite(input.attackSeconds) ||
      !std::isfinite(input.durationSeconds) ||
      !std::isfinite(input.releaseSeconds) ||
      !std::isfinite(input.decaySeconds) ||
      !std::isfinite(input.initialPhaseCycles) ||
      !std::isfinite(input.secondHarmonicGain) ||
      !std::isfinite(input.secondHarmonicPhaseCycles) ||
      !std::isfinite(input.gain) ||
      input.startFrequencyHz <= 0.0F || input.endFrequencyHz <= 0.0F ||
      input.attackSeconds < 0.0F || input.releaseSeconds < 0.0F ||
      input.durationSeconds <= 0.0F ||
      (input.envelopeCurve == EnvelopeCurve::Exponential &&
       input.decaySeconds <= 0.0F)) {
    return false;
  }

  output.waveform = input.waveform;
  output.startFrequencyHz = std::max(
      kMinimumFrequencyHz, std::min(kMaximumFrequencyHz, input.startFrequencyHz));
  output.endFrequencyHz = std::max(
      kMinimumFrequencyHz, std::min(kMaximumFrequencyHz, input.endFrequencyHz));
  output.gain = clamp01(input.gain);
  output.initialPhaseCycles =
      input.initialPhaseCycles - std::floor(input.initialPhaseCycles);
  output.secondHarmonicGain =
      std::max(-1.0F, std::min(1.0F, input.secondHarmonicGain));
  output.secondHarmonicPhaseCycles = input.secondHarmonicPhaseCycles -
                                     std::floor(input.secondHarmonicPhaseCycles);
  output.envelopeCurve = input.envelopeCurve;
  output.attackMultiplier =
      input.envelopeCurve == EnvelopeCurve::Exponential
          ? std::exp(-1.0F /
                     (std::max(input.attackSeconds, 0.00000001F) *
                      kSampleRate))
          : 0.0F;
  output.decayMultiplier =
      input.envelopeCurve == EnvelopeCurve::Exponential
          ? std::exp(-1.0F / (input.decaySeconds * kSampleRate))
          : 1.0F;
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
  voice.phase = tone.initialPhaseCycles;
  voice.attackComplement = 1.0F;
  voice.decayEnvelope = 1.0F;
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
    case Waveform::Sine:
      wave = wavetableSine(voice.phase);
      if (tone.secondHarmonicGain != 0.0F) {
        float harmonicPhase =
            voice.phase * 2.0F + tone.secondHarmonicPhaseCycles;
        while (harmonicPhase >= 1.0F) harmonicPhase -= 1.0F;
        wave += tone.secondHarmonicGain * wavetableSine(harmonicPhase);
      }
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
  if (tone.envelopeCurve == EnvelopeCurve::Exponential) {
    envelope = (1.0F - voice.attackComplement) * voice.decayEnvelope;
  } else if (voice.ageSamples < tone.attackSamples) {
    envelope = static_cast<float>(voice.ageSamples) /
               static_cast<float>(tone.attackSamples);
  }
  if (voice.ageSamples >= tone.totalSamples - tone.releaseSamples) {
    const std::uint32_t remaining =
        tone.totalSamples - 1U - voice.ageSamples;
    envelope *= tone.releaseSamples > 1U
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
  if (tone.envelopeCurve == EnvelopeCurve::Exponential) {
    voice.attackComplement *= tone.attackMultiplier;
    voice.decayEnvelope *= tone.decayMultiplier;
  }
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
