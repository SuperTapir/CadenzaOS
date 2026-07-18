#include "i2s_audio_output.h"

#include <array>

#include "board_pins.h"
#include "cadenza/audio/i2s_audio_format.h"

namespace {
constexpr i2s_port_t kPort = I2S_NUM_0;
constexpr std::size_t kFramesPerChunk = 128;
constexpr TickType_t kWriteTimeout = pdMS_TO_TICKS(20);
constexpr UBaseType_t kTaskPriority = 2;
constexpr std::uint32_t kTaskStackBytes = 4096;
}  // namespace

bool I2sAudioOutput::begin(cadenza::AppRuntime& runtime,
                           cadenza::DiagnosticSink* diagnostics) noexcept {
  if (running()) return true;
  runtime_ = &runtime;
  diagnostics_ = diagnostics;

  i2s_config_t config{};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = static_cast<std::uint32_t>(cadenza::audio::kSampleRate);
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 4;
  config.dma_buf_len = kFramesPerChunk;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;

  esp_err_t result = i2s_driver_install(kPort, &config, 0, nullptr);
  if (result != ESP_OK) {
    emit(cadenza::DiagnosticCode::AudioFailure,
         "I2S driver installation failed", result);
    return false;
  }
  driverInstalled_ = true;

  const i2s_pin_config_t pins{I2S_PIN_NO_CHANGE, BoardPins::kI2sBclk,
                              BoardPins::kI2sWclk, BoardPins::kI2sDataOut,
                              I2S_PIN_NO_CHANGE};
  result = i2s_set_pin(kPort, &pins);
  if (result != ESP_OK) {
    emit(cadenza::DiagnosticCode::AudioFailure,
         "I2S pin configuration failed", result);
    i2s_driver_uninstall(kPort);
    driverInstalled_ = false;
    return false;
  }

  running_.store(true, std::memory_order_release);
  TaskHandle_t createdTask = nullptr;
  const BaseType_t created = xTaskCreatePinnedToCore(
      taskEntry, "cadenza-audio", kTaskStackBytes, this, kTaskPriority,
      &createdTask, 0);
  if (created != pdPASS) {
    running_.store(false, std::memory_order_release);
    emit(cadenza::DiagnosticCode::AudioFailure,
         "audio task creation failed", created);
    i2s_driver_uninstall(kPort);
    driverInstalled_ = false;
    return false;
  }
  task_.store(createdTask, std::memory_order_release);
  emit(cadenza::DiagnosticCode::AudioInitialized,
       "I2S audio task started");
  return true;
}

void I2sAudioOutput::stop() noexcept {
  running_.store(false, std::memory_order_release);
  for (std::uint8_t attempt = 0; task_.load(std::memory_order_acquire) != nullptr &&
                                 attempt < 20;
       ++attempt) {
    delay(5);
  }
  if (driverInstalled_) {
    i2s_zero_dma_buffer(kPort);
    i2s_driver_uninstall(kPort);
    driverInstalled_ = false;
  }
}

void I2sAudioOutput::taskEntry(void* context) noexcept {
  static_cast<I2sAudioOutput*>(context)->run();
}

void I2sAudioOutput::run() noexcept {
  std::array<std::int16_t, kFramesPerChunk> mono{};
  std::array<cadenza::audio::StereoI2sFrame, kFramesPerChunk> stereo{};
  bool partialWriteReported = false;
  while (running_.load(std::memory_order_acquire)) {
    runtime_->renderAudio(mono.data(), mono.size());
    cadenza::audio::duplicateMonoToStereo(mono.data(), stereo.data(),
                                          mono.size());
    std::size_t written = 0;
    const esp_err_t result =
        i2s_write(kPort, stereo.data(), sizeof(stereo), &written, kWriteTimeout);
    if (result != ESP_OK) {
      emit(cadenza::DiagnosticCode::AudioFailure, "I2S write failed", result);
      running_.store(false, std::memory_order_release);
      break;
    }
    if (written != sizeof(stereo) && !partialWriteReported) {
      emit(cadenza::DiagnosticCode::AudioUnderrun,
           "I2S write was incomplete", static_cast<std::int32_t>(written));
      partialWriteReported = true;
    } else if (written == sizeof(stereo)) {
      partialWriteReported = false;
    }
  }
  task_.store(nullptr, std::memory_order_release);
  vTaskDelete(nullptr);
}

void I2sAudioOutput::emit(cadenza::DiagnosticCode code, const char* message,
                          std::int32_t value) const noexcept {
  if (diagnostics_) {
    diagnostics_->emit(
        {cadenza::DiagnosticCategory::Audio, code, message, value});
  }
}
