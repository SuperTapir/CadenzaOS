#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/voice/voice_input.h"

namespace cadenza::voice {

enum class VoiceDmaAlignment : std::uint8_t {
  MostSignificant,
  LeastSignificant,
};

enum class VoiceDmaChannelMode : std::uint8_t {
  First,
  Second,
  Average,
};

struct VoiceDmaFormat {
  std::uint32_t sampleRateHz = kVoiceSampleRateHz;
  std::uint8_t validBits = 32;
  std::uint8_t channels = 2;
  VoiceDmaAlignment alignment = VoiceDmaAlignment::MostSignificant;
  VoiceDmaChannelMode channelMode = VoiceDmaChannelMode::First;
};

struct VoiceDmaNormalizerConfig {
  VoiceDmaFormat input{};
  std::uint8_t maxConsecutiveReadFailures = 3;
};

struct VoiceDmaNormalizerDiagnostics {
  std::uint64_t slotsAccepted = 0;
  std::uint64_t framesAccepted = 0;
  std::uint32_t blocksPublished = 0;
  std::uint32_t publishOverruns = 0;
  std::uint32_t publishRejected = 0;
  std::uint32_t timeouts = 0;
  std::uint32_t readErrors = 0;
  std::uint32_t fatalReadFailures = 0;
  std::uint32_t invalidInputs = 0;
};

// Converts signed samples carried in 32-bit DMA slots into the platform's
// fixed 48 kHz/S16/mono/10 ms block contract. This class performs no I/O and
// allocates no memory, so the hardware read loop remains separately testable.
class VoiceDmaNormalizer {
 public:
  VoiceDmaNormalizer(VoiceCaptureCoordinator& capture,
                     VoiceDmaNormalizerConfig config = {}) noexcept;

  bool valid() const noexcept { return valid_; }
  bool ingest(const std::int32_t* slots, std::size_t slotCount) noexcept;
  void notifyTimeout() noexcept;
  void notifyReadError() noexcept;
  void reset() noexcept;

  const VoiceDmaNormalizerDiagnostics& diagnostics() const noexcept {
    return diagnostics_;
  }
  std::size_t pendingFrameSlots() const noexcept { return frameSlotCount_; }
  std::size_t pendingBlockSamples() const noexcept { return blockSampleCount_; }

 private:
  static constexpr std::size_t kMaxChannels = 4;

  void acceptFrame() noexcept;
  std::int16_t normalize(std::int64_t sample) const noexcept;
  void publishBlock() noexcept;
  void noteReadFailure(bool timeout) noexcept;
  void clearPending() noexcept;

  VoiceCaptureCoordinator& capture_;
  VoiceDmaNormalizerConfig config_;
  bool valid_ = false;
  std::array<std::int32_t, kMaxChannels> frameSlots_{};
  std::size_t frameSlotCount_ = 0;
  VoiceBlock block_{};
  std::size_t blockSampleCount_ = 0;
  std::uint64_t nextSequence_ = 0;
  std::uint8_t consecutiveReadFailures_ = 0;
  VoiceDmaNormalizerDiagnostics diagnostics_{};
};

}  // namespace cadenza::voice
