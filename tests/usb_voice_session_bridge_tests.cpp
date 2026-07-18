#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>

#include "cadenza/voice/usb_voice_session_bridge.h"

namespace {

cadenza::voice::VoiceBlock block(std::int16_t value) {
  cadenza::voice::VoiceBlock result;
  std::fill(result.samples.begin(), result.samples.end(), value);
  return result;
}

}  // namespace

TEST_CASE("control callback lifecycle activates only on consumer synchronization") {
  using namespace cadenza::voice;
  VoiceCaptureCoordinator capture;
  capture.setAvailable(true);
  UsbVoicePacketizer packetizer{capture};
  UsbVoiceSessionBridge bridge{capture};

  bridge.onConnectedFromCallback(true);
  bridge.onAltSettingFromCallback(true);
  CHECK_FALSE(capture.intentActive(VoiceConsumer::Usb));
  CHECK(bridge.synchronize(packetizer));
  CHECK(capture.intentActive(VoiceConsumer::Usb));
  CHECK(capture.state() == VoiceCaptureState::Starting);
  REQUIRE(capture.notifyStarted(kVoicePcmFormat));

  bridge.onAltSettingFromCallback(false);
  CHECK_FALSE(capture.intentActive(VoiceConsumer::Usb));
  CHECK_FALSE(bridge.synchronize(packetizer));
  CHECK_FALSE(packetizer.streaming());
}

TEST_CASE("rapid close and reopen flushes partial and inactive PCM") {
  using namespace cadenza::voice;
  VoiceCaptureCoordinator capture;
  capture.setAvailable(true);
  REQUIRE(capture.setIntent(VoiceConsumer::Analyzer, true));
  REQUIRE(capture.notifyStarted(kVoicePcmFormat));
  UsbVoicePacketizer packetizer{capture};
  UsbVoiceSessionBridge bridge{capture};
  bridge.onConnectedFromCallback(true);
  bridge.onAltSettingFromCallback(true);
  REQUIRE(bridge.synchronize(packetizer));

  REQUIRE(capture.publish(block(100), kVoicePcmFormat) ==
          VoicePublishResult::Accepted);
  UsbVoicePacket packet;
  REQUIRE(packetizer.nextPacket(packet));
  CHECK(packet.samples.front() == 100);

  bridge.onAltSettingFromCallback(false);
  bridge.onAltSettingFromCallback(true);
  REQUIRE(capture.publish(block(200), kVoicePcmFormat) ==
          VoicePublishResult::Accepted);
  REQUIRE(bridge.synchronize(packetizer));
  REQUIRE(packetizer.nextPacket(packet));
  CHECK(std::all_of(packet.samples.begin(), packet.samples.end(),
                    [](std::int16_t sample) { return sample == 0; }));

  REQUIRE(capture.publish(block(300), kVoicePcmFormat) ==
          VoicePublishResult::Accepted);
  REQUIRE(packetizer.nextPacket(packet));
  CHECK(packet.samples.front() == 300);
  CHECK(bridge.diagnostics().lifecycleChanges == 4);
  CHECK(bridge.diagnostics().synchronizations == 2);
}

TEST_CASE("disconnect deactivates USB intent before the next data callback") {
  using namespace cadenza::voice;
  VoiceCaptureCoordinator capture;
  capture.setAvailable(true);
  UsbVoicePacketizer packetizer{capture};
  UsbVoiceSessionBridge bridge{capture};
  bridge.onConnectedFromCallback(true);
  bridge.onAltSettingFromCallback(true);
  REQUIRE(bridge.synchronize(packetizer));
  CHECK(capture.intentActive(VoiceConsumer::Usb));

  bridge.onConnectedFromCallback(false);
  CHECK_FALSE(capture.intentActive(VoiceConsumer::Usb));
  CHECK_FALSE(bridge.connected());
  CHECK_FALSE(bridge.altSettingActive());
}
