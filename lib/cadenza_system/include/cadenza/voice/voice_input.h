#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/voice_types.h"

namespace cadenza::voice {

using VoiceCaptureState = cadenza::VoiceCaptureState;

inline constexpr std::uint32_t kVoiceSampleRateHz = 48000;
inline constexpr std::size_t kVoiceSamplesPerBlock = 480;
inline constexpr std::uint32_t kVoiceBlockDurationMs = 10;

struct VoicePcmFormat {
  std::uint32_t sampleRateHz = kVoiceSampleRateHz;
  std::uint8_t bitsPerSample = 16;
  std::uint8_t channels = 1;
  bool signedSamples = true;

  friend constexpr bool operator==(VoicePcmFormat lhs,
                                   VoicePcmFormat rhs) noexcept {
    return lhs.sampleRateHz == rhs.sampleRateHz &&
           lhs.bitsPerSample == rhs.bitsPerSample &&
           lhs.channels == rhs.channels &&
           lhs.signedSamples == rhs.signedSamples;
  }
  friend constexpr bool operator!=(VoicePcmFormat lhs,
                                   VoicePcmFormat rhs) noexcept {
    return !(lhs == rhs);
  }
};

inline constexpr VoicePcmFormat kVoicePcmFormat{};

struct VoiceBlock {
  std::uint64_t sequence = 0;
  std::array<std::int16_t, kVoiceSamplesPerBlock> samples{};
};

enum class VoiceConsumer : std::uint8_t {
  Analyzer,
  Usb,
};

enum class VoicePublishResult : std::uint8_t {
  Accepted,
  AcceptedWithOverrun,
  NotRunning,
  FormatMismatch,
};

struct VoiceConsumerDiagnostics {
  std::uint32_t published = 0;
  std::uint32_t consumed = 0;
  std::uint32_t overruns = 0;
  std::size_t highWater = 0;
};

struct VoiceCaptureDiagnostics {
  std::uint32_t formatErrors = 0;
  std::uint32_t rejectedBlocks = 0;
};

template <std::size_t Capacity>
class VoiceSpscQueue {
 public:
  static_assert(Capacity > 0, "voice queue capacity must be positive");

  bool push(const VoiceBlock& block) noexcept {
    const std::size_t write = write_.load(std::memory_order_relaxed);
    const std::size_t read = read_.load(std::memory_order_acquire);
    if (write - read >= Capacity) {
      overruns_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    blocks_[write % Capacity] = block;
    write_.store(write + 1, std::memory_order_release);
    published_.fetch_add(1, std::memory_order_relaxed);
    updateHighWater(write + 1 - read);
    return true;
  }

  bool tryPop(VoiceBlock& block) noexcept {
    const std::size_t read = read_.load(std::memory_order_relaxed);
    const std::size_t write = write_.load(std::memory_order_acquire);
    if (read == write) return false;
    block = blocks_[read % Capacity];
    read_.store(read + 1, std::memory_order_release);
    consumed_.fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  void clear() noexcept {
    read_.store(write_.load(std::memory_order_acquire),
                std::memory_order_release);
  }

  VoiceConsumerDiagnostics diagnostics() const noexcept {
    return {published_.load(std::memory_order_relaxed),
            consumed_.load(std::memory_order_relaxed),
            overruns_.load(std::memory_order_relaxed),
            highWater_.load(std::memory_order_relaxed)};
  }

 private:
  void updateHighWater(std::size_t value) noexcept {
    std::size_t observed = highWater_.load(std::memory_order_relaxed);
    while (observed < value &&
           !highWater_.compare_exchange_weak(observed, value,
                                             std::memory_order_relaxed)) {
    }
  }

  std::array<VoiceBlock, Capacity> blocks_{};
  std::atomic<std::size_t> write_{0};
  std::atomic<std::size_t> read_{0};
  std::atomic<std::uint32_t> published_{0};
  std::atomic<std::uint32_t> consumed_{0};
  std::atomic<std::uint32_t> overruns_{0};
  std::atomic<std::size_t> highWater_{0};
};

class VoiceCaptureCoordinator {
 public:
  static constexpr std::size_t kConsumerQueueCapacity = 4;

  void setAvailable(bool available) noexcept;
  bool setIntent(VoiceConsumer consumer, bool active) noexcept;
  bool notifyStarted(VoicePcmFormat format) noexcept;
  void notifyError() noexcept;
  VoicePublishResult publish(const VoiceBlock& block,
                             VoicePcmFormat format) noexcept;
  bool tryPop(VoiceConsumer consumer, VoiceBlock& block) noexcept;

  cadenza::VoiceCaptureState state() const noexcept {
    return state_.load(std::memory_order_acquire);
  }
  bool intentActive(VoiceConsumer consumer) const noexcept;
  bool microphoneInUse() const noexcept;
  VoiceCaptureDiagnostics diagnostics() const noexcept;
  VoiceConsumerDiagnostics consumerDiagnostics(
      VoiceConsumer consumer) const noexcept;

 private:
  VoiceSpscQueue<kConsumerQueueCapacity>& queue(VoiceConsumer consumer) noexcept;
  const VoiceSpscQueue<kConsumerQueueCapacity>& queue(
      VoiceConsumer consumer) const noexcept;
  void clearQueues() noexcept;

  std::atomic<cadenza::VoiceCaptureState> state_{
      cadenza::VoiceCaptureState::Unavailable};
  std::atomic<bool> analyzerIntent_{false};
  std::atomic<bool> usbIntent_{false};
  VoiceSpscQueue<kConsumerQueueCapacity> analyzerQueue_{};
  VoiceSpscQueue<kConsumerQueueCapacity> usbQueue_{};
  std::atomic<std::uint32_t> formatErrors_{0};
  std::atomic<std::uint32_t> rejectedBlocks_{0};
};

struct VoiceAnalyzerConfig {
  float activityThreshold = 0.04F;
  std::uint16_t attackBlocks = 2;
  std::uint16_t releaseBlocks = 5;
};

struct VoiceAnalyzerSnapshot {
  float rms = 0.0F;
  float peak = 0.0F;
  bool voiceActive = false;
  std::uint32_t clippedSamples = 0;
  std::uint64_t lastSequence = 0;
};

class VoiceAnalyzer {
 public:
  explicit VoiceAnalyzer(VoiceAnalyzerConfig config = {}) noexcept;
  void process(const VoiceBlock& block) noexcept;
  void reset() noexcept;
  const VoiceAnalyzerSnapshot& snapshot() const noexcept { return snapshot_; }

 private:
  VoiceAnalyzerConfig config_;
  VoiceAnalyzerSnapshot snapshot_{};
  std::uint16_t attackCount_ = 0;
  std::uint16_t releaseCount_ = 0;
};

}  // namespace cadenza::voice
