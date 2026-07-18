#include "cadenza/voice/voice_dma_normalizer.h"

#include <algorithm>
#include <limits>

namespace cadenza::voice {

namespace {

bool isValid(const VoiceDmaNormalizerConfig& config) noexcept {
  const auto& input = config.input;
  if (input.sampleRateHz != kVoiceSampleRateHz || input.validBits < 16 ||
      input.validBits > 32 || input.channels == 0 ||
      input.channels > 4 ||
      config.maxConsecutiveReadFailures == 0) {
    return false;
  }
  return input.channelMode != VoiceDmaChannelMode::Second ||
         input.channels >= 2;
}

}  // namespace

VoiceDmaNormalizer::VoiceDmaNormalizer(
    VoiceCaptureCoordinator& capture, VoiceDmaNormalizerConfig config) noexcept
    : capture_(capture), config_(config), valid_(isValid(config)) {}

bool VoiceDmaNormalizer::ingest(const std::int32_t* slots,
                                std::size_t slotCount) noexcept {
  if (!valid_ || (slots == nullptr && slotCount != 0)) {
    ++diagnostics_.invalidInputs;
    return false;
  }
  if (slotCount != 0) consecutiveReadFailures_ = 0;
  for (std::size_t index = 0; index < slotCount; ++index) {
    frameSlots_[frameSlotCount_++] = slots[index];
    ++diagnostics_.slotsAccepted;
    if (frameSlotCount_ == config_.input.channels) acceptFrame();
  }
  return true;
}

void VoiceDmaNormalizer::notifyTimeout() noexcept { noteReadFailure(true); }

void VoiceDmaNormalizer::notifyReadError() noexcept { noteReadFailure(false); }

void VoiceDmaNormalizer::reset() noexcept {
  clearPending();
  consecutiveReadFailures_ = 0;
  nextSequence_ = 0;
}

void VoiceDmaNormalizer::acceptFrame() noexcept {
  std::int64_t selected = frameSlots_[0];
  if (config_.input.channelMode == VoiceDmaChannelMode::Second) {
    selected = frameSlots_[1];
  } else if (config_.input.channelMode == VoiceDmaChannelMode::Average) {
    selected = 0;
    for (std::size_t channel = 0; channel < config_.input.channels; ++channel) {
      selected += frameSlots_[channel];
    }
    selected /= config_.input.channels;
  }
  frameSlotCount_ = 0;
  ++diagnostics_.framesAccepted;
  block_.samples[blockSampleCount_++] = normalize(selected);
  if (blockSampleCount_ == block_.samples.size()) publishBlock();
}

std::int16_t VoiceDmaNormalizer::normalize(std::int64_t sample) const noexcept {
  if (config_.input.alignment == VoiceDmaAlignment::MostSignificant) {
    sample /= (std::int64_t{1} << 16);
  } else if (config_.input.validBits > 16) {
    sample /= (std::int64_t{1} << (config_.input.validBits - 16));
  }
  sample = std::clamp(sample,
                      static_cast<std::int64_t>(
                          std::numeric_limits<std::int16_t>::min()),
                      static_cast<std::int64_t>(
                          std::numeric_limits<std::int16_t>::max()));
  return static_cast<std::int16_t>(sample);
}

void VoiceDmaNormalizer::publishBlock() noexcept {
  block_.sequence = nextSequence_++;
  const VoicePublishResult result = capture_.publish(block_, kVoicePcmFormat);
  if (result == VoicePublishResult::Accepted ||
      result == VoicePublishResult::AcceptedWithOverrun) {
    ++diagnostics_.blocksPublished;
    if (result == VoicePublishResult::AcceptedWithOverrun) {
      ++diagnostics_.publishOverruns;
    }
  } else {
    ++diagnostics_.publishRejected;
  }
  blockSampleCount_ = 0;
}

void VoiceDmaNormalizer::noteReadFailure(bool timeout) noexcept {
  if (timeout) {
    ++diagnostics_.timeouts;
  } else {
    ++diagnostics_.readErrors;
  }
  if (consecutiveReadFailures_ <
      std::numeric_limits<std::uint8_t>::max()) {
    ++consecutiveReadFailures_;
  }
  if (consecutiveReadFailures_ >= config_.maxConsecutiveReadFailures) {
    ++diagnostics_.fatalReadFailures;
    consecutiveReadFailures_ = 0;
    clearPending();
    capture_.notifyError();
  }
}

void VoiceDmaNormalizer::clearPending() noexcept {
  frameSlotCount_ = 0;
  blockSampleCount_ = 0;
}

}  // namespace cadenza::voice
