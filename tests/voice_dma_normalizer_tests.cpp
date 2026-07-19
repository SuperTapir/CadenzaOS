#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstdint>

#include "cadenza/voice/voice_dma_normalizer.h"

namespace {

using namespace cadenza::voice;

void startCapture(VoiceCaptureCoordinator& capture, VoiceConsumer consumer) {
  capture.setAvailable(true);
  REQUIRE(capture.setIntent(consumer, true));
  REQUIRE(capture.notifyStarted(kVoicePcmFormat));
}

TEST_CASE("DMA chunks retain partial stereo frames and publish exact blocks") {
  VoiceCaptureCoordinator capture;
  startCapture(capture, VoiceConsumer::Usb);
  VoiceDmaNormalizer normalizer{capture};
  REQUIRE(normalizer.valid());

  std::array<std::int32_t, kVoiceSamplesPerBlock * 2> slots{};
  for (std::size_t frame = 0; frame < kVoiceSamplesPerBlock; ++frame) {
    slots[frame * 2] = static_cast<std::int32_t>(frame << 16);
    slots[frame * 2 + 1] = -123 * 65536;
  }
  CHECK(normalizer.ingest(slots.data(), 17));
  CHECK(normalizer.pendingFrameSlots() == 1);
  CHECK(normalizer.ingest(slots.data() + 17, slots.size() - 17));

  VoiceBlock block;
  REQUIRE(capture.tryPop(VoiceConsumer::Usb, block));
  CHECK(block.sequence == 0);
  for (std::size_t index = 0; index < block.samples.size(); ++index) {
    CHECK(block.samples[index] == static_cast<std::int16_t>(index));
  }
  CHECK_FALSE(capture.tryPop(VoiceConsumer::Usb, block));
  CHECK(normalizer.pendingFrameSlots() == 0);
  CHECK(normalizer.pendingBlockSamples() == 0);
  CHECK(normalizer.diagnostics().blocksPublished == 1);
}

TEST_CASE("channel selection and averaging are deterministic") {
  VoiceCaptureCoordinator capture;
  startCapture(capture, VoiceConsumer::Analyzer);
  VoiceDmaNormalizerConfig config;
  config.input.channelMode = VoiceDmaChannelMode::Second;
  VoiceDmaNormalizer second{capture, config};
  std::array<std::int32_t, kVoiceSamplesPerBlock * 2> slots{};
  for (std::size_t frame = 0; frame < kVoiceSamplesPerBlock; ++frame) {
    slots[frame * 2] = 1000 << 16;
    slots[frame * 2 + 1] = -2000 * 65536;
  }
  REQUIRE(second.ingest(slots.data(), slots.size()));
  VoiceBlock block;
  REQUIRE(capture.tryPop(VoiceConsumer::Analyzer, block));
  CHECK(block.samples.front() == -2000);
  CHECK(block.samples.back() == -2000);

  capture.setIntent(VoiceConsumer::Analyzer, false);
  REQUIRE(capture.setIntent(VoiceConsumer::Analyzer, true));
  REQUIRE(capture.notifyStarted(kVoicePcmFormat));
  config.input.channelMode = VoiceDmaChannelMode::Average;
  VoiceDmaNormalizer average{capture, config};
  REQUIRE(average.ingest(slots.data(), slots.size()));
  REQUIRE(capture.tryPop(VoiceConsumer::Analyzer, block));
  CHECK(block.samples.front() == -500);
  CHECK(block.samples.back() == -500);
}

TEST_CASE("24 and 32 bit slots scale and saturate into signed S16") {
  VoiceCaptureCoordinator capture;
  startCapture(capture, VoiceConsumer::Usb);
  VoiceDmaNormalizerConfig config;
  config.input.channels = 1;
  config.input.validBits = 24;
  config.input.alignment = VoiceDmaAlignment::MostSignificant;
  VoiceDmaNormalizer msb24{capture, config};
  std::array<std::int32_t, kVoiceSamplesPerBlock> slots{};
  slots.fill(static_cast<std::int32_t>(0x7FFFFF00));
  slots[1] = static_cast<std::int32_t>(0x80000000U);
  REQUIRE(msb24.ingest(slots.data(), slots.size()));
  VoiceBlock block;
  REQUIRE(capture.tryPop(VoiceConsumer::Usb, block));
  CHECK(block.samples[0] == 32767);
  CHECK(block.samples[1] == -32768);

  capture.setIntent(VoiceConsumer::Usb, false);
  REQUIRE(capture.setIntent(VoiceConsumer::Usb, true));
  REQUIRE(capture.notifyStarted(kVoicePcmFormat));
  config.input.validBits = 16;
  config.input.alignment = VoiceDmaAlignment::LeastSignificant;
  VoiceDmaNormalizer lsb16{capture, config};
  slots.fill(100000);
  slots[1] = -100000;
  REQUIRE(lsb16.ingest(slots.data(), slots.size()));
  REQUIRE(capture.tryPop(VoiceConsumer::Usb, block));
  CHECK(block.samples[0] == 32767);
  CHECK(block.samples[1] == -32768);
}

TEST_CASE("read failures are bounded and discard incomplete data on fatal error") {
  VoiceCaptureCoordinator capture;
  startCapture(capture, VoiceConsumer::Usb);
  VoiceDmaNormalizer normalizer{capture};
  const std::int32_t oneSlot = 42 << 16;
  REQUIRE(normalizer.ingest(&oneSlot, 1));
  CHECK(normalizer.pendingFrameSlots() == 1);

  normalizer.notifyTimeout();
  CHECK(normalizer.pendingFrameSlots() == 0);
  normalizer.notifyReadError();
  CHECK(normalizer.pendingFrameSlots() == 0);
  CHECK(capture.state() == VoiceCaptureState::Running);
  normalizer.notifyTimeout();
  CHECK(capture.state() == VoiceCaptureState::Error);
  CHECK(normalizer.pendingFrameSlots() == 0);
  CHECK(normalizer.pendingBlockSamples() == 0);
  CHECK(normalizer.diagnostics().timeouts == 2);
  CHECK(normalizer.diagnostics().readErrors == 1);
  CHECK(normalizer.diagnostics().fatalReadFailures == 1);
}

TEST_CASE("invalid formats and null input fail without touching capture") {
  VoiceCaptureCoordinator capture;
  startCapture(capture, VoiceConsumer::Usb);
  VoiceDmaNormalizerConfig config;
  config.input.sampleRateHz = 44100;
  VoiceDmaNormalizer invalidRate{capture, config};
  CHECK_FALSE(invalidRate.valid());
  CHECK_FALSE(invalidRate.ingest(nullptr, 1));
  CHECK(capture.state() == VoiceCaptureState::Running);

  config.input.sampleRateHz = kVoiceSampleRateHz;
  config.input.channels = 1;
  config.input.channelMode = VoiceDmaChannelMode::Second;
  VoiceDmaNormalizer invalidChannel{capture, config};
  CHECK_FALSE(invalidChannel.valid());
}

}  // namespace
