#pragma once

#include <cstdint>

#include "cadenza/system/system_service_host.h"

namespace cadenza::host {

enum class HeadlessVoicePattern : std::uint8_t {
  Silence,
  Constant,
  Sine,
  SpeechBurst,
};

struct HeadlessMicrophoneConfig {
  HeadlessVoicePattern pattern = HeadlessVoicePattern::Silence;
  std::int16_t amplitude = 12000;
  std::uint32_t frequencyHz = 1000;
  std::uint32_t speechBlocks = 4;
  std::uint32_t silenceBlocks = 6;
};

struct HeadlessMicrophoneDiagnostics {
  std::uint64_t generatedBlocks = 0;
  std::uint64_t injectedErrors = 0;
  voice::VoicePublishResult lastPublish = voice::VoicePublishResult::NotRunning;
};

class HeadlessMicrophone {
 public:
  explicit HeadlessMicrophone(
      system::SystemServiceHost& services,
      HeadlessMicrophoneConfig config = {}) noexcept
      : services_(&services), config_(config) {}

  bool connect() noexcept;
  bool disconnect() noexcept;
  bool notifyStarted() noexcept;
  bool produceBlock() noexcept;
  void setConfig(HeadlessMicrophoneConfig config) noexcept { config_ = config; }
  void injectErrorAt(std::uint64_t sequence) noexcept {
    errorSequence_ = sequence;
  }
  void clearInjectedError() noexcept { errorSequence_ = kNoError; }

  std::uint64_t nextSequence() const noexcept { return nextSequence_; }
  const voice::VoiceBlock& lastBlock() const noexcept { return lastBlock_; }
  const HeadlessMicrophoneDiagnostics& diagnostics() const noexcept {
    return diagnostics_;
  }

 private:
  static constexpr std::uint64_t kNoError = ~std::uint64_t{0};
  void generate() noexcept;

  system::SystemServiceHost* services_ = nullptr;
  HeadlessMicrophoneConfig config_{};
  voice::VoiceBlock lastBlock_{};
  std::uint64_t nextSequence_ = 0;
  std::uint64_t errorSequence_ = kNoError;
  double phase_ = 0.0;
  HeadlessMicrophoneDiagnostics diagnostics_{};
};

}  // namespace cadenza::host
