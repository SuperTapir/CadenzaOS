#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>

#include "cadenza/host/headless_host.h"

namespace {

void startAnalyzer(cadenza::host::HeadlessHost& host) {
  REQUIRE(host.microphone().connect());
  host.step();
  REQUIRE(host.services().submit(
      cadenza::SystemCommand::setVoiceAnalyzerActive(true)));
  host.step();
  REQUIRE(host.microphone().notifyStarted());
  host.step();
  REQUIRE(host.services().snapshot().voice.captureState ==
          cadenza::VoiceCaptureState::Running);
}

}  // namespace

TEST_CASE("headless sine replay is deterministic and feeds analyzer snapshot") {
  cadenza::host::HeadlessMicrophoneConfig config;
  config.pattern = cadenza::host::HeadlessVoicePattern::Sine;
  config.amplitude = 12000;
  config.frequencyHz = 1000;

  cadenza::host::HeadlessHost first{cadenza::FramebufferProfile::TEmbed};
  cadenza::host::HeadlessHost second{cadenza::FramebufferProfile::TEmbed};
  first.microphone().setConfig(config);
  second.microphone().setConfig(config);
  startAnalyzer(first);
  startAnalyzer(second);

  for (int index = 0; index < 2; ++index) {
    REQUIRE(first.microphone().produceBlock());
    REQUIRE(second.microphone().produceBlock());
  }
  const auto firstHash = cadenza::host::pcmHash(
      first.microphone().lastBlock().samples.data(),
      first.microphone().lastBlock().samples.size());
  const auto secondHash = cadenza::host::pcmHash(
      second.microphone().lastBlock().samples.data(),
      second.microphone().lastBlock().samples.size());
  CHECK(firstHash == secondHash);
  first.step();
  second.step();
  CHECK(first.services().snapshot().voice.rms ==
        doctest::Approx(second.services().snapshot().voice.rms));
  CHECK(first.services().snapshot().voice.voiceActive);
}

TEST_CASE("headless silence constant and speech burst fixtures are explicit") {
  cadenza::host::HeadlessHost host{cadenza::FramebufferProfile::TEmbed};
  startAnalyzer(host);
  cadenza::host::HeadlessMicrophoneConfig config;
  config.pattern = cadenza::host::HeadlessVoicePattern::Silence;
  host.microphone().setConfig(config);
  REQUIRE(host.microphone().produceBlock());
  CHECK(std::all_of(host.microphone().lastBlock().samples.begin(),
                    host.microphone().lastBlock().samples.end(),
                    [](std::int16_t sample) { return sample == 0; }));

  config.pattern = cadenza::host::HeadlessVoicePattern::Constant;
  config.amplitude = 3210;
  host.microphone().setConfig(config);
  REQUIRE(host.microphone().produceBlock());
  CHECK(std::all_of(host.microphone().lastBlock().samples.begin(),
                    host.microphone().lastBlock().samples.end(),
                    [](std::int16_t sample) { return sample == 3210; }));

  config.pattern = cadenza::host::HeadlessVoicePattern::SpeechBurst;
  config.speechBlocks = 1;
  config.silenceBlocks = 1;
  host.microphone().setConfig(config);
  REQUIRE(host.microphone().produceBlock());
  const bool firstAudible = std::any_of(
      host.microphone().lastBlock().samples.begin(),
      host.microphone().lastBlock().samples.end(),
      [](std::int16_t sample) { return sample != 0; });
  REQUIRE(host.microphone().produceBlock());
  const bool secondAudible = std::any_of(
      host.microphone().lastBlock().samples.begin(),
      host.microphone().lastBlock().samples.end(),
      [](std::int16_t sample) { return sample != 0; });
  CHECK(firstAudible != secondAudible);
}

TEST_CASE("headless overrun and injected error remain bounded and observable") {
  cadenza::host::HeadlessHost host{cadenza::FramebufferProfile::TEmbed};
  cadenza::host::HeadlessMicrophoneConfig config;
  config.pattern = cadenza::host::HeadlessVoicePattern::Constant;
  host.microphone().setConfig(config);
  startAnalyzer(host);

  for (std::size_t index = 0;
       index < cadenza::voice::VoiceCaptureCoordinator::kConsumerQueueCapacity +
                   1;
       ++index) {
    REQUIRE(host.microphone().produceBlock());
  }
  CHECK(host.services()
            .voiceCapture()
            .consumerDiagnostics(cadenza::voice::VoiceConsumer::Analyzer)
            .overruns == 1);

  host.microphone().injectErrorAt(host.microphone().nextSequence());
  REQUIRE(host.microphone().produceBlock());
  host.step();
  CHECK(host.services().snapshot().voice.captureState ==
        cadenza::VoiceCaptureState::Error);
  CHECK(host.microphone().diagnostics().injectedErrors == 1);
}
