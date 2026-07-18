#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <thread>

#include "cadenza/audio/interaction_sound.h"
#include "cadenza/core/app_runtime.h"
#include "cadenza/core/mono_canvas.h"
#include "cadenza/system/connectivity_ingress.h"
#include "cadenza/system/frame_coordinator.h"
#include "cadenza/voice/voice_dma_normalizer.h"
#include "cadenza_t_embed_audio_contract.h"

namespace {
class FrameProbeApp final : public cadenza::App {
 public:
  const char* name() const noexcept override { return "FrameProbe"; }
  void update(const cadenza::AppUpdateContext&) noexcept override { ++updates; }
  void render(cadenza::MonoCanvas& canvas,
              const cadenza::AppRenderContext&) noexcept override {
    canvas.pixel(updates % canvas.width(), 0, true);
  }
  std::uint32_t updates = 0;
};
}  // namespace

TEST_CASE("T-Embed speaker and microphone own disjoint realtime resources") {
  using namespace cadenza::t_embed_audio;
  CHECK(kSpeaker.i2sPort != kMicrophone.i2sPort);
  CHECK(kSpeaker.taskCore != kMicrophone.taskCore);
  CHECK(kSpeaker.dmaDescriptors == 4);
  CHECK(kMicrophone.dmaDescriptors == 4);
  CHECK(kSpeaker.dmaFrames == 128);
  CHECK(kMicrophone.dmaFrames == 240);
  CHECK(kSpeaker.taskStackBytes == 4096);
  CHECK(kMicrophone.taskStackBytes == 6144);
  CHECK(resourcesAreIndependent());
}

TEST_CASE("speaker pressure cannot block or overwrite microphone timeline") {
  cadenza::audio::InteractionSoundService sound;
  cadenza::voice::VoiceCaptureCoordinator capture;
  capture.setAvailable(true);
  REQUIRE(capture.setIntent(cadenza::voice::VoiceConsumer::Analyzer, true));
  REQUIRE(capture.notifyStarted(cadenza::voice::kVoicePcmFormat));

  cadenza::voice::VoiceDmaNormalizerConfig normalizerConfig;
  normalizerConfig.input.sampleRateHz = cadenza::voice::kVoiceSampleRateHz;
  normalizerConfig.input.validBits = 32;
  normalizerConfig.input.channels = 2;
  normalizerConfig.input.alignment =
      cadenza::voice::VoiceDmaAlignment::MostSignificant;
  normalizerConfig.input.channelMode =
      cadenza::voice::VoiceDmaChannelMode::Average;
  cadenza::voice::VoiceDmaNormalizer normalizer{capture, normalizerConfig};
  REQUIRE(normalizer.valid());

  std::array<std::int32_t, cadenza::voice::kVoiceSamplesPerBlock * 2> micDma{};
  for (std::size_t frame = 0;
       frame < cadenza::voice::kVoiceSamplesPerBlock; ++frame) {
    const std::int32_t sample = static_cast<std::int32_t>(frame + 1) << 16;
    micDma[frame * 2] = sample;
    micDma[frame * 2 + 1] = sample;
  }

  for (int cue = 0; cue < 64; ++cue) {
    sound.play(cadenza::audio::SoundCue::Confirm);
  }
  std::array<std::int16_t, cadenza::t_embed_audio::kSpeaker.dmaFrames>
      speakerBuffer{};
  sound.render(speakerBuffer.data(), speakerBuffer.size());
  REQUIRE(normalizer.ingest(micDma.data(), micDma.size()) == 1);

  cadenza::voice::VoiceBlock block;
  REQUIRE(capture.tryPop(cadenza::voice::VoiceConsumer::Analyzer, block));
  for (std::size_t index = 0; index < block.samples.size(); ++index) {
    CAPTURE(index);
    CHECK(block.samples[index] == static_cast<std::int16_t>(index + 1));
  }
  CHECK(sound.overflowCount() > 0);
  CHECK(capture.consumerDiagnostics(cadenza::voice::VoiceConsumer::Analyzer)
            .overruns == 0);
}

TEST_CASE("callback bursts and USB voice do not starve 60 Hz App frames") {
  using namespace cadenza::system;
  SystemServiceHost host;
  auto& connectivity = host.connectivityService();
  connectivity.setWiFiAvailable(true);
  connectivity.requestNetworkOnline(true);
  ConnectivityAction action;
  REQUIRE(connectivity.tryPopAction(action));
  connectivity.onWiFiRadioReady();
  REQUIRE(connectivity.tryPopAction(action));
  const std::uint32_t wifiGeneration = action.generation;

  connectivity.setBluetoothLeAvailable(true);
  connectivity.requestBluetoothScanning(true);
  REQUIRE(connectivity.tryPopAction(action));
  const std::uint32_t bluetoothGeneration = action.generation;
  connectivity.onBluetoothLeRadioReady(bluetoothGeneration);
  REQUIRE(connectivity.tryPopAction(action));

  REQUIRE(host.postPlatformEvent(PlatformEvent::microphoneAvailability(true)));
  REQUIRE(host.postPlatformEvent(PlatformEvent::usbVoiceStreaming(true)));
  REQUIRE(host.postPlatformEvent(PlatformEvent::voiceCaptureStarted()));
  host.beginFrame(0.0F);

  FrameProbeApp app;
  cadenza::AppRuntime runtime;
  constexpr cadenza::AppId kAppId{0x7701};
  REQUIRE(runtime.registerApp(kAppId, app, false));
  REQUIRE(runtime.begin(kAppId));
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  ConnectivityIngressMux<64, 64> ingress;

  std::thread wifiProducer([&] {
    for (std::uint32_t index = 0; index < 4096; ++index) {
      const PlatformEvent event =
          (index & 1U) == 0
              ? PlatformEvent::wifiAssociated(wifiGeneration)
              : PlatformEvent::wifiGotIp(wifiGeneration);
      ingress.postWiFiFromCallback(event);
    }
  });
  std::thread bluetoothProducer([&] {
    cadenza::BluetoothLeSnapshot snapshot;
    snapshot.radio = cadenza::BluetoothLeRadioState::Ready;
    snapshot.generation = bluetoothGeneration;
    snapshot.scanning = true;
    for (std::uint32_t index = 0; index < 4096; ++index) {
      snapshot.connectionCount = static_cast<std::uint8_t>(index & 1U);
      ingress.postBluetoothFromCallback(
          PlatformEvent::bluetoothObserved(snapshot));
    }
  });
  std::thread voiceProducer([&] {
    cadenza::voice::VoiceBlock block;
    for (std::uint32_t sequence = 0; sequence < 4096; ++sequence) {
      block.sequence = sequence;
      block.samples[0] = static_cast<std::int16_t>(sequence);
      host.publishVoiceBlock(block);
    }
  });

  std::uint32_t usbBlocks = 0;
  for (std::uint32_t frame = 0; frame < 120; ++frame) {
    ingress.drainFrame(host, 4, 4);
    FrameCoordinator::runFrame(host, runtime, canvas, 1.0F / 60.0F, {});
    cadenza::voice::VoiceBlock block;
    while (host.voiceCapture().tryPop(cadenza::voice::VoiceConsumer::Usb,
                                      block)) {
      ++usbBlocks;
    }
    std::this_thread::yield();
  }
  wifiProducer.join();
  bluetoothProducer.join();
  voiceProducer.join();
  ingress.drainFrame(host, 0, 0);
  host.beginFrame(0.0F);

  CHECK(app.updates == 120);
  CHECK(usbBlocks > 0);
  CHECK(ingress.wifiDiagnostics().rejected > 0);
  CHECK(ingress.bluetoothDiagnostics().rejected > 0);
  CHECK(host.snapshot().connectivity.wifi.resyncNeeded);
  CHECK(host.snapshot().connectivity.bluetoothLe.resyncNeeded);
}
