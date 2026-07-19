#include "cadenza/audio/sound_cue_library.h"

namespace cadenza::audio {
namespace {
constexpr float kOneSample = 1.0F / kSampleRate;

ToneSpec linearTone(Waveform waveform, float startHz, float endHz,
                    float duration, float gain, std::uint8_t priority,
                    float attack = 0.002F,
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

ToneSpec resonator(float startHz, float endHz, float duration, float gain,
                   float decay, float phaseCycles, std::uint8_t priority,
                   float attack = 0.00022F,
                   float release = 0.0008F,
                   float secondHarmonicGain = 0.0F,
                   float secondHarmonicPhaseCycles = 0.06366198F) noexcept {
  ToneSpec value = linearTone(Waveform::Sine, startHz, endHz, duration, gain,
                              priority, attack, release);
  value.envelopeCurve = EnvelopeCurve::Exponential;
  value.decaySeconds = decay;
  value.initialPhaseCycles = phaseCycles;
  value.secondHarmonicGain = secondHarmonicGain;
  value.secondHarmonicPhaseCycles = secondHarmonicPhaseCycles;
  return value;
}

SoundEvent event(const ToneSpec& tone, float delay = 0.0F) noexcept {
  return {tone, delay};
}

// Navigate is the approved Select waveform-fit v6. Four compact, freshly
// authored damped resonators reproduce the measured pressure impulse without
// embedding any reference PCM. Delay + one-sample attack puts the first
// audible sample at frame 169, matching the approved timing.
const SoundCueDefinition kNavigate{{{
    event(resonator(2290.0F, 2290.0F, 120.0F / kSampleRate, 0.7699553F,
                    0.0004099503F, 0.07371847F, 0, 0.000001F, kOneSample),
          168.0F / kSampleRate),
    event(resonator(5835.0F, 5835.0F, 120.0F / kSampleRate, 0.21705055F,
                    0.0002434536F, 0.77908583F, 0, 0.000001F, kOneSample),
          168.0F / kSampleRate),
    event(resonator(1635.0F, 1635.0F, 120.0F / kSampleRate, 0.39631617F,
                    0.0002898256F, 0.55121913F, 0, 0.000001F, kOneSample),
          168.0F / kSampleRate),
    event(resonator(2845.0F, 2845.0F, 120.0F / kSampleRate, 0.18355016F,
                    0.0004975957F, 0.59541461F, 0, 0.000001F, kOneSample),
          168.0F / kSampleRate),
}}, 4};

const std::array<SoundCueDefinition,
                 static_cast<std::size_t>(SoundCue::Count)>
    kPalette{{
        kNavigate,
        {{{event(linearTone(Waveform::Triangle, 520.0F, 390.0F, 0.082F,
                            0.22F, 1, 0.001F, 0.008F))}},
         1},
        {{{event(resonator(659.0F, 659.0F, 0.170F, 0.428F, 0.064F, 0.015F,
                           2, 0.00045F, 0.015F)),
           event(resonator(988.0F, 988.0F, 0.181F, 0.357F, 0.080F, 0.031F,
                           2, 0.00032F, 0.015F),
                 0.064F)}},
         2},
        {{{event(resonator(988.0F, 988.0F, 0.130F, 0.425F, 0.048F, 0.023F,
                           2, 0.00045F, 0.015F)),
           event(resonator(587.0F, 587.0F, 0.165F, 0.421F, 0.060F, 0.434F,
                           2, 0.0015F, 0.015F),
                 0.045F)}},
         2},
        {{{event(resonator(3300.0F, 3780.0F, 0.0105F, 1.0F, 0.00155F,
                           0.0748535F, 1, 0.00022F, 0.0008F, 0.10F),
                 0.0035F),
           event(resonator(1500.0F, 2520.0F, 0.0115F, 0.68F, 0.0019F,
                           0.129552F, 1, 0.00022F, 0.0008F, 0.16F),
                 0.044F),
           event(resonator(4550.0F, 5850.0F, 0.0060F, 0.32F, 0.00095F,
                           0.294272F, 1, 0.00022F, 0.0008F, 0.03F),
                 0.045F)}},
         3},
        {{{event(resonator(3780.0F, 3220.0F, 0.0105F, 1.0F, 0.00155F,
                           0.0856879F, 1, 0.00022F, 0.0008F, 0.10F),
                 0.006F),
           event(resonator(2380.0F, 1320.0F, 0.0115F, 0.70F, 0.0019F,
                           0.197161F, 1, 0.00022F, 0.0008F, 0.16F),
                 0.0475F),
           event(resonator(5850.0F, 4100.0F, 0.0058F, 0.30F, 0.0009F,
                           0.180243F, 1, 0.00022F, 0.0008F, 0.03F),
                 0.049F)}},
         3},
        {{{event(linearTone(Waveform::Noise, 900.0F, 500.0F, 0.060F,
                            0.10F, 3, 0.001F, 0.008F)),
           event(resonator(560.0F, 330.0F, 0.105F, 0.20F, 0.040F, 0.35F,
                           3, 0.001F, 0.010F))}},
         2},
        {{{event(resonator(523.25F, 659.25F, 0.220F, 0.22F, 0.090F, 0.0F,
                           2, 0.003F, 0.012F)),
           event(resonator(659.25F, 783.99F, 0.220F, 0.19F, 0.090F, 0.08F,
                           2, 0.003F, 0.012F),
                 0.110F),
           event(resonator(783.99F, 1046.50F, 0.220F, 0.17F, 0.095F, 0.16F,
                           2, 0.003F, 0.014F),
                 0.220F)}},
         3},
        {{{event(resonator(660.0F, 620.0F, 0.160F, 0.23F, 0.055F, 0.10F,
                           2, 0.002F, 0.010F)),
           event(resonator(660.0F, 600.0F, 0.160F, 0.21F, 0.055F, 0.10F,
                           2, 0.002F, 0.010F),
                 0.160F)}},
         2},
        {{{event(resonator(620.0F, 520.0F, 0.150F, 0.23F, 0.060F, 0.15F,
                           3, 0.002F, 0.010F)),
           event(resonator(520.0F, 420.0F, 0.150F, 0.21F, 0.060F, 0.25F,
                           3, 0.002F, 0.010F),
                 0.120F),
           event(resonator(420.0F, 320.0F, 0.150F, 0.20F, 0.065F, 0.35F,
                           3, 0.002F, 0.012F),
                 0.240F)}},
         3},
        {{{event(resonator(587.33F, 587.33F, 0.260F, 0.16F, 0.110F, 0.0F,
                           1, 0.006F, 0.015F)),
           event(resonator(739.99F, 739.99F, 0.260F, 0.13F, 0.105F, 0.12F,
                           1, 0.006F, 0.015F)),
           event(resonator(880.00F, 880.00F, 0.260F, 0.10F, 0.100F, 0.24F,
                           1, 0.006F, 0.015F))}},
         3},
        {{{event(linearTone(Waveform::Triangle, 420.0F, 480.0F, 0.050F,
                            0.18F, 2, 0.001F, 0.006F)),
           event(resonator(520.0F, 820.0F, 0.210F, 0.23F, 0.085F, 0.0F,
                           2, 0.002F, 0.012F),
                 0.080F)}},
         2},
        {{{event(resonator(820.0F, 620.0F, 0.160F, 0.23F, 0.065F, 0.10F,
                           2, 0.002F, 0.010F)),
           event(resonator(560.0F, 380.0F, 0.155F, 0.20F, 0.065F, 0.50F,
                           2, 0.002F, 0.010F),
                 0.090F)}},
         2},
        {{{event(resonator(293.66F, 293.66F, 0.500F, 0.30F, 0.220F, 0.0F,
                           2, 0.004F, 0.020F)),
           event(resonator(440.00F, 440.00F, 0.450F, 0.23F, 0.190F, 0.10F,
                           2, 0.004F, 0.020F),
                 0.100F),
           event(resonator(554.37F, 554.37F, 0.390F, 0.19F, 0.170F, 0.20F,
                           2, 0.004F, 0.020F),
                 0.220F),
           event(resonator(739.99F, 739.99F, 0.430F, 0.15F, 0.150F, 0.30F,
                           2, 0.004F, 0.020F),
                 0.350F)}},
         4},
        {{{event(resonator(659.25F, 659.25F, 0.380F, 0.21F, 0.170F, 0.20F,
                           2, 0.004F, 0.018F)),
           event(resonator(440.00F, 440.00F, 0.350F, 0.25F, 0.160F, 0.35F,
                           2, 0.004F, 0.018F),
                 0.120F),
           event(resonator(293.66F, 293.66F, 0.390F, 0.30F, 0.140F, 0.50F,
                           2, 0.004F, 0.018F),
                 0.270F)}},
         3},
        {{{event(resonator(523.25F, 659.25F, 0.160F, 0.30F, 0.060F, 0.00F,
                           3, 0.002F, 0.012F)),
           event(resonator(659.25F, 783.99F, 0.160F, 0.28F, 0.060F, 0.11F,
                           3, 0.002F, 0.012F),
                 0.180F),
           event(resonator(523.25F, 659.25F, 0.160F, 0.30F, 0.060F, 0.22F,
                           3, 0.002F, 0.012F),
                 0.360F),
           event(resonator(783.99F, 1046.50F, 0.160F, 0.27F, 0.065F, 0.33F,
                           3, 0.002F, 0.014F),
                 0.540F)}},
         4},
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
    case SoundCue::Complete:
      return "complete";
    case SoundCue::Warning:
      return "warning";
    case SoundCue::Failure:
      return "failure";
    case SoundCue::Notification:
      return "notification";
    case SoundCue::Connect:
      return "connect";
    case SoundCue::Disconnect:
      return "disconnect";
    case SoundCue::PowerOn:
      return "power-on";
    case SoundCue::PowerOff:
      return "power-off";
    case SoundCue::TimerComplete:
      return "timer-complete";
    case SoundCue::Count:
      return "invalid";
  }
  return "invalid";
}

}  // namespace cadenza::audio
