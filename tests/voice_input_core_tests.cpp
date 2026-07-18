#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "cadenza/voice/voice_input.h"

namespace {

cadenza::voice::VoiceBlock ramp(std::uint64_t sequence,
                                std::int16_t first = 0) {
  cadenza::voice::VoiceBlock block;
  block.sequence = sequence;
  for (std::size_t index = 0; index < block.samples.size(); ++index) {
    block.samples[index] = static_cast<std::int16_t>(first + index);
  }
  return block;
}

}  // namespace

TEST_CASE("voice PCM contract is explicit and preserves all 480 S16 samples") {
  using namespace cadenza::voice;
  static_assert(kVoiceSampleRateHz == 48000);
  static_assert(kVoiceSamplesPerBlock == 480);
  static_assert(kVoiceBlockDurationMs == 10);
  static_assert(sizeof(VoiceBlock{}.samples[0]) == 2);

  VoiceCaptureCoordinator capture;
  capture.setAvailable(true);
  REQUIRE(capture.setIntent(VoiceConsumer::Analyzer, true));
  CHECK(capture.state() == VoiceCaptureState::Starting);
  REQUIRE(capture.notifyStarted(kVoicePcmFormat));
  CHECK(capture.state() == VoiceCaptureState::Running);

  const VoiceBlock input = ramp(7, -200);
  REQUIRE(capture.publish(input, kVoicePcmFormat) ==
          VoicePublishResult::Accepted);
  VoiceBlock output;
  REQUIRE(capture.tryPop(VoiceConsumer::Analyzer, output));
  CHECK(output.sequence == 7);
  CHECK(output.samples == input.samples);

  VoicePcmFormat stereo = kVoicePcmFormat;
  stereo.channels = 2;
  CHECK(capture.publish(input, stereo) ==
        VoicePublishResult::FormatMismatch);
  CHECK(capture.state() == VoiceCaptureState::Error);
  CHECK(capture.diagnostics().formatErrors == 1);
}

TEST_CASE("slow consumer drops newest independently and later recovers") {
  using namespace cadenza::voice;
  VoiceCaptureCoordinator capture;
  capture.setAvailable(true);
  REQUIRE(capture.setIntent(VoiceConsumer::Analyzer, true));
  REQUIRE(capture.setIntent(VoiceConsumer::Usb, true));
  REQUIRE(capture.notifyStarted(kVoicePcmFormat));

  for (std::uint64_t sequence = 0;
       sequence < VoiceCaptureCoordinator::kConsumerQueueCapacity;
       ++sequence) {
    REQUIRE(capture.publish(ramp(sequence), kVoicePcmFormat) ==
            VoicePublishResult::Accepted);
    VoiceBlock usb;
    REQUIRE(capture.tryPop(VoiceConsumer::Usb, usb));
    CHECK(usb.sequence == sequence);
  }

  REQUIRE(capture.publish(ramp(99), kVoicePcmFormat) ==
          VoicePublishResult::AcceptedWithOverrun);
  VoiceBlock usb;
  REQUIRE(capture.tryPop(VoiceConsumer::Usb, usb));
  CHECK(usb.sequence == 99);
  CHECK(capture.consumerDiagnostics(VoiceConsumer::Analyzer).overruns == 1);
  CHECK(capture.consumerDiagnostics(VoiceConsumer::Usb).overruns == 0);

  VoiceBlock analyzer;
  REQUIRE(capture.tryPop(VoiceConsumer::Analyzer, analyzer));
  CHECK(analyzer.sequence == 0);
  REQUIRE(capture.publish(ramp(100), kVoicePcmFormat) ==
          VoicePublishResult::Accepted);
  while (capture.tryPop(VoiceConsumer::Analyzer, analyzer)) {
  }
  CHECK(analyzer.sequence == 100);
}

TEST_CASE("capture state follows aggregate consumer intent and flushes stale data") {
  using namespace cadenza::voice;
  VoiceCaptureCoordinator capture;
  CHECK_FALSE(capture.setIntent(VoiceConsumer::Usb, true));
  CHECK(capture.state() == VoiceCaptureState::Unavailable);
  capture.setAvailable(true);
  REQUIRE(capture.setIntent(VoiceConsumer::Analyzer, true));
  REQUIRE(capture.setIntent(VoiceConsumer::Usb, true));
  REQUIRE(capture.notifyStarted(kVoicePcmFormat));
  REQUIRE(capture.publish(ramp(1), kVoicePcmFormat) ==
          VoicePublishResult::Accepted);
  capture.setIntent(VoiceConsumer::Analyzer, false);
  CHECK(capture.state() == VoiceCaptureState::Running);
  capture.setIntent(VoiceConsumer::Usb, false);
  CHECK(capture.state() == VoiceCaptureState::Stopped);
  VoiceBlock stale;
  CHECK_FALSE(capture.tryPop(VoiceConsumer::Usb, stale));
}

TEST_CASE("analyzer reports deterministic RMS peak clipping and VAD hysteresis") {
  using namespace cadenza::voice;
  VoiceAnalyzer analyzer{{0.10F, 2, 3}};
  VoiceBlock silence{};
  analyzer.process(silence);
  CHECK(analyzer.snapshot().rms == doctest::Approx(0.0F));
  CHECK(analyzer.snapshot().peak == doctest::Approx(0.0F));
  CHECK_FALSE(analyzer.snapshot().voiceActive);

  VoiceBlock loud{};
  std::fill(loud.samples.begin(), loud.samples.end(), 16384);
  loud.samples[0] = -32768;
  analyzer.process(loud);
  CHECK(analyzer.snapshot().rms > 0.49F);
  CHECK(analyzer.snapshot().peak == doctest::Approx(1.0F));
  CHECK(analyzer.snapshot().clippedSamples == 1);
  CHECK_FALSE(analyzer.snapshot().voiceActive);
  analyzer.process(loud);
  CHECK(analyzer.snapshot().voiceActive);

  analyzer.process(silence);
  analyzer.process(silence);
  CHECK(analyzer.snapshot().voiceActive);
  analyzer.process(silence);
  CHECK_FALSE(analyzer.snapshot().voiceActive);
}

TEST_CASE("long burst keeps USB ordered while stalled analyzer reports bounded loss") {
  using namespace cadenza::voice;
  VoiceCaptureCoordinator capture;
  capture.setAvailable(true);
  REQUIRE(capture.setIntent(VoiceConsumer::Analyzer, true));
  REQUIRE(capture.setIntent(VoiceConsumer::Usb, true));
  REQUIRE(capture.notifyStarted(kVoicePcmFormat));

  VoiceBlock output;
  for (std::uint64_t sequence = 0; sequence < 10000; ++sequence) {
    capture.publish(ramp(sequence), kVoicePcmFormat);
    REQUIRE(capture.tryPop(VoiceConsumer::Usb, output));
    REQUIRE(output.sequence == sequence);
    if (sequence % 11 == 0) capture.tryPop(VoiceConsumer::Analyzer, output);
  }
  const auto analyzer = capture.consumerDiagnostics(VoiceConsumer::Analyzer);
  const auto usb = capture.consumerDiagnostics(VoiceConsumer::Usb);
  CHECK(analyzer.overruns > 0);
  CHECK(analyzer.highWater ==
        VoiceCaptureCoordinator::kConsumerQueueCapacity);
  CHECK(usb.overruns == 0);
  CHECK(usb.highWater == 1);

  while (capture.tryPop(VoiceConsumer::Analyzer, output)) {
  }
  REQUIRE(capture.publish(ramp(10000), kVoicePcmFormat) ==
          VoicePublishResult::Accepted);
  REQUIRE(capture.tryPop(VoiceConsumer::Analyzer, output));
  CHECK(output.sequence == 10000);
}
