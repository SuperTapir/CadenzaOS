#include "cadenza/host/headless_microphone.h"

#include <algorithm>
#include <cmath>

namespace cadenza::host {

bool HeadlessMicrophone::connect() noexcept {
  return services_ && services_->postPlatformEvent(
                          system::PlatformEvent::microphoneAvailability(true));
}

bool HeadlessMicrophone::disconnect() noexcept {
  return services_ && services_->postPlatformEvent(
                          system::PlatformEvent::microphoneAvailability(false));
}

bool HeadlessMicrophone::notifyStarted() noexcept {
  return services_ && services_->postPlatformEvent(
                          system::PlatformEvent::voiceCaptureStarted());
}

bool HeadlessMicrophone::produceBlock() noexcept {
  if (!services_) return false;
  if (nextSequence_ == errorSequence_) {
    ++nextSequence_;
    ++diagnostics_.injectedErrors;
    return services_->postPlatformEvent(
        system::PlatformEvent::voiceCaptureError());
  }
  generate();
  diagnostics_.lastPublish = services_->publishVoiceBlock(lastBlock_);
  ++diagnostics_.generatedBlocks;
  ++nextSequence_;
  return diagnostics_.lastPublish == voice::VoicePublishResult::Accepted ||
         diagnostics_.lastPublish ==
             voice::VoicePublishResult::AcceptedWithOverrun;
}

void HeadlessMicrophone::generate() noexcept {
  lastBlock_ = {};
  lastBlock_.sequence = nextSequence_;
  bool audible = config_.pattern != HeadlessVoicePattern::Silence;
  if (config_.pattern == HeadlessVoicePattern::SpeechBurst) {
    const std::uint64_t cycle =
        std::max<std::uint64_t>(1, config_.speechBlocks + config_.silenceBlocks);
    audible = nextSequence_ % cycle < config_.speechBlocks;
  }
  if (!audible) return;
  if (config_.pattern == HeadlessVoicePattern::Constant) {
    lastBlock_.samples.fill(config_.amplitude);
    return;
  }

  constexpr double kTwoPi = 6.283185307179586476925286766559;
  const double step = kTwoPi * static_cast<double>(config_.frequencyHz) /
                      static_cast<double>(voice::kVoiceSampleRateHz);
  for (std::int16_t& sample : lastBlock_.samples) {
    sample = static_cast<std::int16_t>(
        std::lround(std::sin(phase_) * config_.amplitude));
    phase_ += step;
    if (phase_ >= kTwoPi) phase_ -= kTwoPi;
  }
}

}  // namespace cadenza::host
