#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstdint>

#include "firmware_stubs.h"
#include "i2s_audio_output.h"

namespace {
class RecordingDiagnostics final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    events[count < events.size() ? count : events.size() - 1] = event;
    if (count < events.size()) ++count;
  }

  bool contains(cadenza::DiagnosticCode code) const {
    for (std::size_t index = 0; index < count; ++index) {
      if (events[index].code == code) return true;
    }
    return false;
  }

  std::array<cadenza::DiagnosticEvent, 8> events{};
  std::size_t count = 0;
};
}  // namespace

TEST_CASE("I2S configuration uses the locked format pins and bounded buffers") {
  firmware_stubs::reset();
  cadenza::audio::InteractionSoundService service;
  RecordingDiagnostics diagnostics;
  I2sAudioOutput output;
  REQUIRE(output.begin(service, &diagnostics));
  const auto& state = firmware_stubs::state();
  CHECK(state.config.sample_rate == 44100);
  CHECK(state.config.bits_per_sample == I2S_BITS_PER_SAMPLE_16BIT);
  CHECK(state.config.channel_format == I2S_CHANNEL_FMT_RIGHT_LEFT);
  CHECK(state.config.dma_buf_count == 4);
  CHECK(state.config.dma_buf_len == 128);
  CHECK(state.pins.bck_io_num == 7);
  CHECK(state.pins.ws_io_num == 5);
  CHECK(state.pins.data_out_num == 6);
  CHECK(diagnostics.contains(cadenza::DiagnosticCode::AudioInitialized));
  output.stop();
  CHECK(state.uninstallCalls == 1);
}

TEST_CASE("I2S install pin and task failures disable audio without Runtime loss") {
  for (int failure = 0; failure < 3; ++failure) {
    CAPTURE(failure);
    firmware_stubs::reset();
    auto& state = firmware_stubs::state();
    if (failure == 0) state.installResult = -10;
    if (failure == 1) state.pinResult = -11;
    if (failure == 2) state.taskResult = 0;
    cadenza::audio::InteractionSoundService service;
    RecordingDiagnostics diagnostics;
    I2sAudioOutput output;
    CHECK_FALSE(output.begin(service, &diagnostics));
    CHECK_FALSE(output.running());
    CHECK(diagnostics.contains(cadenza::DiagnosticCode::AudioFailure));

    REQUIRE(service.play(cadenza::audio::SoundCue::Confirm));
    std::array<std::int16_t, 256> samples{};
    service.render(samples.data(), samples.size());
    CHECK(service.pendingCommandCount() == 0);
    if (failure > 0) CHECK(state.uninstallCalls == 1);
  }
}

TEST_CASE("fatal I2S write stops the task and reports failure") {
  firmware_stubs::reset();
  firmware_stubs::state().writeResult = -12;
  cadenza::audio::InteractionSoundService service;
  RecordingDiagnostics diagnostics;
  I2sAudioOutput output;
  REQUIRE(output.begin(service, &diagnostics));
  REQUIRE(firmware_stubs::state().capturedTask != nullptr);
  firmware_stubs::state().capturedTask(
      firmware_stubs::state().capturedContext);
  CHECK_FALSE(output.running());
  CHECK(firmware_stubs::state().writeCalls == 1);
  CHECK(diagnostics.contains(cadenza::DiagnosticCode::AudioFailure));
  output.stop();
}

TEST_CASE("partial I2S write is diagnosed once before a fatal retry") {
  firmware_stubs::reset();
  auto& state = firmware_stubs::state();
  state.writeBytes = 64;
  state.writeResultAfterFirst = -13;
  cadenza::audio::InteractionSoundService service;
  RecordingDiagnostics diagnostics;
  I2sAudioOutput output;
  REQUIRE(output.begin(service, &diagnostics));
  state.capturedTask(state.capturedContext);
  CHECK(state.writeCalls == 2);
  CHECK(diagnostics.contains(cadenza::DiagnosticCode::AudioUnderrun));
  CHECK(diagnostics.contains(cadenza::DiagnosticCode::AudioFailure));
  output.stop();
}
