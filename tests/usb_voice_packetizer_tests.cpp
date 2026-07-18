#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cstdint>

#include "cadenza/voice/usb_voice_packetizer.h"

namespace {
cadenza::voice::VoiceBlock sequential(std::uint64_t sequence,
                                      std::int16_t offset) {
  cadenza::voice::VoiceBlock block;
  block.sequence = sequence;
  for (std::size_t index = 0; index < block.samples.size(); ++index) {
    block.samples[index] = static_cast<std::int16_t>(offset + index);
  }
  return block;
}
}  // namespace

TEST_CASE("one voice block becomes ten ordered one millisecond packets") {
  using namespace cadenza::voice;
  VoiceCaptureCoordinator capture;
  capture.setAvailable(true);
  UsbVoicePacketizer usb{capture};
  usb.setConnected(true);
  usb.setAltSettingActive(true);
  REQUIRE(capture.notifyStarted(kVoicePcmFormat));
  const VoiceBlock input = sequential(1, -240);
  REQUIRE(capture.publish(input, kVoicePcmFormat) ==
          VoicePublishResult::Accepted);

  std::array<std::int16_t, kVoiceSamplesPerBlock> reconstructed{};
  for (std::size_t packetIndex = 0; packetIndex < 10; ++packetIndex) {
    UsbVoicePacket packet;
    REQUIRE(usb.nextPacket(packet));
    for (std::size_t sample = 0; sample < packet.samples.size(); ++sample) {
      reconstructed[packetIndex * packet.samples.size() + sample] =
          packet.samples[sample];
    }
  }
  CHECK(reconstructed == input.samples);
  CHECK(usb.diagnostics().packets == 10);
  CHECK(usb.diagnostics().underflows == 0);
}

TEST_CASE("active underflow is fixed length silence and never repeats old PCM") {
  using namespace cadenza::voice;
  VoiceCaptureCoordinator capture;
  capture.setAvailable(true);
  UsbVoicePacketizer usb{capture};
  usb.setConnected(true);
  usb.setAltSettingActive(true);
  REQUIRE(capture.notifyStarted(kVoicePcmFormat));

  UsbVoicePacket packet;
  REQUIRE(usb.nextPacket(packet));
  CHECK(std::all_of(packet.samples.begin(), packet.samples.end(),
                    [](std::int16_t sample) { return sample == 0; }));
  CHECK(usb.diagnostics().underflows == 1);

  REQUIRE(capture.publish(sequential(2, 100), kVoicePcmFormat) ==
          VoicePublishResult::Accepted);
  for (int index = 0; index < 10; ++index) REQUIRE(usb.nextPacket(packet));
  REQUIRE(usb.nextPacket(packet));
  CHECK(std::all_of(packet.samples.begin(), packet.samples.end(),
                    [](std::int16_t sample) { return sample == 0; }));
  CHECK(usb.diagnostics().underflows == 2);
}

TEST_CASE("inactive and reconnect flush stale blocks and partial packet state") {
  using namespace cadenza::voice;
  VoiceCaptureCoordinator capture;
  capture.setAvailable(true);
  REQUIRE(capture.setIntent(VoiceConsumer::Analyzer, true));
  REQUIRE(capture.notifyStarted(kVoicePcmFormat));
  UsbVoicePacketizer usb{capture};
  usb.setConnected(true);
  usb.setAltSettingActive(true);
  REQUIRE(capture.publish(sequential(3, 300), kVoicePcmFormat) ==
          VoicePublishResult::Accepted);
  UsbVoicePacket packet;
  REQUIRE(usb.nextPacket(packet));
  CHECK(packet.samples[0] == 300);

  usb.setAltSettingActive(false);
  CHECK_FALSE(usb.nextPacket(packet));
  REQUIRE(capture.publish(sequential(4, 400), kVoicePcmFormat) ==
          VoicePublishResult::Accepted);
  usb.setAltSettingActive(true);
  REQUIRE(usb.nextPacket(packet));
  CHECK(std::all_of(packet.samples.begin(), packet.samples.end(),
                    [](std::int16_t sample) { return sample == 0; }));

  REQUIRE(capture.publish(sequential(5, 500), kVoicePcmFormat) ==
          VoicePublishResult::Accepted);
  REQUIRE(usb.nextPacket(packet));
  CHECK(packet.samples[0] == 500);
  usb.setConnected(false);
  CHECK_FALSE(capture.intentActive(VoiceConsumer::Usb));
  usb.shutdown();
  CHECK_FALSE(usb.nextPacket(packet));
}
