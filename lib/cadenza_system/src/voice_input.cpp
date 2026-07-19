#include "cadenza/voice/voice_input.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace cadenza::voice {

void VoiceCaptureCoordinator::setAvailable(bool available) noexcept {
  if (!available) {
    analyzerIntent_.store(false, std::memory_order_release);
    usbIntent_.store(false, std::memory_order_release);
    clearQueues();
    state_.store(VoiceCaptureState::Unavailable, std::memory_order_release);
    return;
  }
  if (state() == VoiceCaptureState::Unavailable ||
      state() == VoiceCaptureState::Error) {
    state_.store(microphoneInUse() ? VoiceCaptureState::Starting
                                  : VoiceCaptureState::Stopped,
                 std::memory_order_release);
  }
}

bool VoiceCaptureCoordinator::setIntent(VoiceConsumer consumer,
                                        bool active) noexcept {
  if (active && (state() == VoiceCaptureState::Unavailable ||
                 state() == VoiceCaptureState::Error)) {
    return false;
  }
  std::atomic<bool>& intent = consumer == VoiceConsumer::Analyzer
                                  ? analyzerIntent_
                                  : usbIntent_;
  if (active) {
    const bool wasActive = intent.load(std::memory_order_acquire);
    if (!wasActive) {
      queue(consumer).clear();
      intent.store(true, std::memory_order_release);
    }
    if (state() == VoiceCaptureState::Stopped) {
      clearQueues();
      state_.store(VoiceCaptureState::Starting, std::memory_order_release);
    } else if (!wasActive && consumer == VoiceConsumer::Usb &&
               state() == VoiceCaptureState::Running) {
      // USB remount while another consumer kept capture running must recycle
      // I²S/DMA framing; otherwise a sticky stereo slot slip sounds harsh.
      clearQueues();
      state_.store(VoiceCaptureState::Starting, std::memory_order_release);
    }
    return true;
  }
  intent.store(false, std::memory_order_release);
  queue(consumer).clear();
  if (!microphoneInUse()) {
    clearQueues();
    state_.store(VoiceCaptureState::Stopped, std::memory_order_release);
  }
  return true;
}

bool VoiceCaptureCoordinator::notifyStarted(VoicePcmFormat format) noexcept {
  if (format != kVoicePcmFormat) {
    formatErrors_.fetch_add(1, std::memory_order_relaxed);
    state_.store(VoiceCaptureState::Error, std::memory_order_release);
    return false;
  }
  if (state() != VoiceCaptureState::Starting || !microphoneInUse()) {
    return false;
  }
  state_.store(VoiceCaptureState::Running, std::memory_order_release);
  return true;
}

void VoiceCaptureCoordinator::notifyError() noexcept {
  clearQueues();
  state_.store(VoiceCaptureState::Error, std::memory_order_release);
}

VoicePublishResult VoiceCaptureCoordinator::publish(
    const VoiceBlock& block, VoicePcmFormat format) noexcept {
  if (format != kVoicePcmFormat) {
    formatErrors_.fetch_add(1, std::memory_order_relaxed);
    rejectedBlocks_.fetch_add(1, std::memory_order_relaxed);
    notifyError();
    return VoicePublishResult::FormatMismatch;
  }
  if (state() != VoiceCaptureState::Running) {
    rejectedBlocks_.fetch_add(1, std::memory_order_relaxed);
    return VoicePublishResult::NotRunning;
  }

  bool overrun = false;
  if (analyzerIntent_.load(std::memory_order_acquire)) {
    overrun = !analyzerQueue_.push(block) || overrun;
  }
  if (usbIntent_.load(std::memory_order_acquire)) {
    overrun = !usbQueue_.push(block) || overrun;
  }
  return overrun ? VoicePublishResult::AcceptedWithOverrun
                 : VoicePublishResult::Accepted;
}

bool VoiceCaptureCoordinator::tryPop(VoiceConsumer consumer,
                                     VoiceBlock& block) noexcept {
  return queue(consumer).tryPop(block);
}

bool VoiceCaptureCoordinator::intentActive(VoiceConsumer consumer) const
    noexcept {
  return (consumer == VoiceConsumer::Analyzer ? analyzerIntent_ : usbIntent_)
      .load(std::memory_order_acquire);
}

bool VoiceCaptureCoordinator::microphoneInUse() const noexcept {
  return analyzerIntent_.load(std::memory_order_acquire) ||
         usbIntent_.load(std::memory_order_acquire);
}

VoiceCaptureDiagnostics VoiceCaptureCoordinator::diagnostics() const noexcept {
  return {formatErrors_.load(std::memory_order_relaxed),
          rejectedBlocks_.load(std::memory_order_relaxed)};
}

VoiceConsumerDiagnostics VoiceCaptureCoordinator::consumerDiagnostics(
    VoiceConsumer consumer) const noexcept {
  return queue(consumer).diagnostics();
}

VoiceSpscQueue<VoiceCaptureCoordinator::kConsumerQueueCapacity>&
VoiceCaptureCoordinator::queue(VoiceConsumer consumer) noexcept {
  return consumer == VoiceConsumer::Analyzer ? analyzerQueue_ : usbQueue_;
}

const VoiceSpscQueue<VoiceCaptureCoordinator::kConsumerQueueCapacity>&
VoiceCaptureCoordinator::queue(VoiceConsumer consumer) const noexcept {
  return consumer == VoiceConsumer::Analyzer ? analyzerQueue_ : usbQueue_;
}

void VoiceCaptureCoordinator::clearQueues() noexcept {
  analyzerQueue_.clear();
  usbQueue_.clear();
}

VoiceAnalyzer::VoiceAnalyzer(VoiceAnalyzerConfig config) noexcept
    : config_(config) {
  config_.activityThreshold =
      std::clamp(config_.activityThreshold, 0.0F, 1.0F);
  config_.attackBlocks = std::max<std::uint16_t>(config_.attackBlocks, 1);
  config_.releaseBlocks = std::max<std::uint16_t>(config_.releaseBlocks, 1);
}

void VoiceAnalyzer::process(const VoiceBlock& block) noexcept {
  double sumSquares = 0.0;
  std::uint32_t peakMagnitude = 0;
  std::uint32_t clipped = 0;
  for (const std::int16_t sample : block.samples) {
    const std::int32_t widened = sample;
    const std::uint32_t magnitude = static_cast<std::uint32_t>(
        widened < 0 ? -widened : widened);
    peakMagnitude = std::max(peakMagnitude, magnitude);
    clipped += sample == std::numeric_limits<std::int16_t>::min() ||
                       sample == std::numeric_limits<std::int16_t>::max()
                   ? 1U
                   : 0U;
    const double normalized = static_cast<double>(widened) / 32768.0;
    sumSquares += normalized * normalized;
  }

  snapshot_.rms = static_cast<float>(
      std::sqrt(sumSquares / static_cast<double>(block.samples.size())));
  snapshot_.peak =
      std::min(1.0F, static_cast<float>(peakMagnitude) / 32768.0F);
  snapshot_.clippedSamples = clipped;
  snapshot_.lastSequence = block.sequence;

  if (snapshot_.rms >= config_.activityThreshold) {
    releaseCount_ = 0;
    if (attackCount_ < config_.attackBlocks) ++attackCount_;
    if (attackCount_ >= config_.attackBlocks) snapshot_.voiceActive = true;
  } else {
    attackCount_ = 0;
    if (snapshot_.voiceActive && releaseCount_ < config_.releaseBlocks) {
      ++releaseCount_;
    }
    if (releaseCount_ >= config_.releaseBlocks) snapshot_.voiceActive = false;
  }
}

void VoiceAnalyzer::reset() noexcept {
  snapshot_ = {};
  attackCount_ = 0;
  releaseCount_ = 0;
}

}  // namespace cadenza::voice
