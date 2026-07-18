#pragma once

#include <cstdint>

#include "cadenza/voice/voice_dma_normalizer.h"
#include "cadenza_t_embed_audio_contract.h"
#include "esp_err.h"

namespace cadenza::idf {

struct TEmbedMicrophoneConfig {
  int i2cPort = 0;
  int i2sPort = t_embed_audio::kMicrophone.i2sPort;
  int sdaPin = 18;
  int sclPin = 8;
  int bclkPin = t_embed_audio::kMicrophoneBclkPin;
  int lrckPin = t_embed_audio::kMicrophoneWordSelectPin;
  int dataInPin = t_embed_audio::kMicrophoneDataInPin;
  int mclkPin = t_embed_audio::kMicrophoneMclkPin;
  std::uint8_t microphoneMask = 0x03;
  float inputGainDb = 30.0F;
  std::uint32_t readTimeoutMs = 20;
};

// ESP-IDF-only ownership boundary for the original T-Embed microphone.
// Portable code owns PCM semantics; this adapter owns buses, codec and DMA.
class TEmbedMicrophone {
 public:
  explicit TEmbedMicrophone(
      voice::VoiceCaptureCoordinator& capture,
      TEmbedMicrophoneConfig config = {}) noexcept;
  ~TEmbedMicrophone();

  TEmbedMicrophone(const TEmbedMicrophone&) = delete;
  TEmbedMicrophone& operator=(const TEmbedMicrophone&) = delete;

  esp_err_t start() noexcept;
  void run() noexcept;
  void stop() noexcept;

 private:
  void release() noexcept;

  voice::VoiceCaptureCoordinator& capture_;
  TEmbedMicrophoneConfig config_;
  voice::VoiceDmaNormalizer normalizer_;
  void* i2cBus_ = nullptr;
  void* rxChannel_ = nullptr;
  const void* controlInterface_ = nullptr;
  const void* dataInterface_ = nullptr;
  const void* codecInterface_ = nullptr;
  void* codecDevice_ = nullptr;
  bool started_ = false;
};

}  // namespace cadenza::idf
