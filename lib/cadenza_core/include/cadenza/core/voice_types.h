#pragma once

#include <cstdint>

namespace cadenza {

enum class VoiceCaptureState : std::uint8_t {
  Unavailable,
  Stopped,
  Starting,
  Running,
  Error,
};

struct VoiceSnapshot {
  VoiceCaptureState captureState = VoiceCaptureState::Unavailable;
  bool available = false;
  bool analyzerActive = false;
  bool usbActive = false;
  bool microphoneInUse = false;
  bool voiceActive = false;
  float rms = 0.0F;
  float peak = 0.0F;
  std::uint32_t analyzerOverruns = 0;
  std::uint32_t usbOverruns = 0;
};

}  // namespace cadenza
