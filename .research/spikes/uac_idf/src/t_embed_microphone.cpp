#include "t_embed_microphone.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "driver/i2c_master.h"
#include "driver/i2s_std.h"
#include "esp_codec_dev.h"
#include "esp_codec_dev_defaults.h"
#include "esp_log.h"
#include "es7210_adc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace cadenza::idf {

namespace {

constexpr char kTag[] = "cadenza_mic";
constexpr std::size_t kDmaBytesPerRead = 512;  // 64 stereo int32 frames
constexpr int kPrimeReadsAfterEnable = 2;
constexpr int kResyncAttempts = 8;
constexpr int kLayoutPickAttempts = 6;

voice::VoiceDmaNormalizerConfig normalizerConfig() noexcept {
  voice::VoiceDmaNormalizerConfig config;
  config.input.sampleRateHz = voice::kVoiceSampleRateHz;
  config.input.validBits = 16;
  config.input.channels = 2;
  // expandVoiceDmaSlots always emits LSB-aligned int16 in int32 containers.
  config.input.alignment = voice::VoiceDmaAlignment::LeastSignificant;
  config.input.channelMode = voice::VoiceDmaChannelMode::First;
  config.maxConsecutiveReadFailures = 3;
  return config;
}

const char* layoutName(voice::VoiceDmaLayout layout) noexcept {
  switch (layout) {
    case voice::VoiceDmaLayout::Int16Interleaved:
      return "i16";
    case voice::VoiceDmaLayout::Int32Msb16:
      return "i32msb";
    case voice::VoiceDmaLayout::Int32Lsb16:
      return "i32lsb";
  }
  return "?";
}

bool enableRx(i2s_chan_handle_t rx, bool& channelEnabled) noexcept {
  if (channelEnabled) {
    (void)i2s_channel_disable(rx);
    channelEnabled = false;
  }
  const esp_err_t enabled = i2s_channel_enable(rx);
  if (enabled != ESP_OK && enabled != ESP_ERR_INVALID_STATE) return false;
  channelEnabled = true;
  return true;
}

bool readRawWindow(i2s_chan_handle_t rx, std::uint8_t* bytes,
                   std::size_t capacity, std::uint32_t timeoutMs,
                   std::size_t* bytesReadOut) noexcept {
  std::size_t bytesRead = 0;
  const esp_err_t result =
      i2s_channel_read(rx, bytes, capacity, &bytesRead, timeoutMs);
  if (result != ESP_OK || bytesRead == 0) return false;
  if (bytesReadOut != nullptr) *bytesReadOut = bytesRead;
  return true;
}

bool primeAndPickLayout(i2s_chan_handle_t rx, bool& channelEnabled,
                        std::uint8_t* bytes, std::size_t capacity,
                        std::uint32_t timeoutMs, voice::VoiceLayoutChoice* out,
                        std::size_t* bytesReadOut) noexcept {
  voice::VoiceLayoutChoice best{};
  std::size_t bestBytes = 0;
  for (int attempt = 0; attempt < kLayoutPickAttempts; ++attempt) {
    if (!enableRx(rx, channelEnabled)) continue;
    std::size_t lastBytes = 0;
    int accepted = 0;
    for (int spin = 0; spin < kPrimeReadsAfterEnable + 8 &&
                       accepted < kPrimeReadsAfterEnable;
         ++spin) {
      std::size_t bytesRead = 0;
      if (!readRawWindow(rx, bytes, capacity, timeoutMs, &bytesRead)) continue;
      // Prefer multiples of 8 bytes so int32 stereo frames are possible.
      if (bytesRead < 16 || bytesRead % 4 != 0) continue;
      lastBytes = bytesRead;
      ++accepted;
    }
    if (accepted < kPrimeReadsAfterEnable || lastBytes == 0) {
      if (channelEnabled) {
        (void)i2s_channel_disable(rx);
        channelEnabled = false;
      }
      continue;
    }
    const auto choice = voice::pickBestVoiceLayout(bytes, lastBytes);
    if (choice.quality.score > best.quality.score) {
      best = choice;
      bestBytes = lastBytes;
    }
    // Good enough speech/continuity — stop early.
    if (best.quality.score >= 0.8F) break;
  }
  if (best.quality.score < 0.0F || bestBytes == 0) return false;
  if (out != nullptr) *out = best;
  if (bytesReadOut != nullptr) *bytesReadOut = bestBytes;
  return true;
}

}  // namespace

TEmbedMicrophone::TEmbedMicrophone(voice::VoiceCaptureCoordinator& capture,
                                   TEmbedMicrophoneConfig config) noexcept
    : capture_(capture),
      config_(config),
      normalizer_(capture, normalizerConfig()) {}

TEmbedMicrophone::~TEmbedMicrophone() { release(); }

esp_err_t TEmbedMicrophone::start() noexcept {
  if (started_) return ESP_OK;
  if (!normalizer_.valid()) return ESP_ERR_INVALID_STATE;

  i2c_master_bus_config_t i2cConfig{};
  i2cConfig.i2c_port = static_cast<i2c_port_num_t>(config_.i2cPort);
  i2cConfig.sda_io_num = static_cast<gpio_num_t>(config_.sdaPin);
  i2cConfig.scl_io_num = static_cast<gpio_num_t>(config_.sclPin);
  i2cConfig.clk_source = I2C_CLK_SRC_DEFAULT;
  i2cConfig.glitch_ignore_cnt = 7;
  i2cConfig.flags.enable_internal_pullup = true;
  auto* bus = reinterpret_cast<i2c_master_bus_handle_t*>(&i2cBus_);
  esp_err_t result = i2c_new_master_bus(&i2cConfig, bus);
  if (result != ESP_OK) {
    release();
    return result;
  }

  i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(
      static_cast<i2s_port_t>(config_.i2sPort), I2S_ROLE_MASTER);
  channelConfig.dma_desc_num = t_embed_audio::kMicrophone.dmaDescriptors;
  channelConfig.dma_frame_num = t_embed_audio::kMicrophone.dmaFrames;
  auto* rx = reinterpret_cast<i2s_chan_handle_t*>(&rxChannel_);
  result = i2s_new_channel(&channelConfig, nullptr, rx);
  if (result != ESP_OK) {
    release();
    return result;
  }

  // Keep 32-bit stereo slots; layout picker decides MSB/LSB/int16 unpack.
  i2s_std_config_t standardConfig{
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(voice::kVoiceSampleRateHz),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
          I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
      .gpio_cfg =
          {
              .mclk = static_cast<gpio_num_t>(config_.mclkPin),
              .bclk = static_cast<gpio_num_t>(config_.bclkPin),
              .ws = static_cast<gpio_num_t>(config_.lrckPin),
              .dout = I2S_GPIO_UNUSED,
              .din = static_cast<gpio_num_t>(config_.dataInPin),
              .invert_flags = {},
          },
  };
  standardConfig.clk_cfg.mclk_multiple = I2S_MCLK_MULTIPLE_256;
  result = i2s_channel_init_std_mode(
      reinterpret_cast<i2s_chan_handle_t>(rxChannel_), &standardConfig);
  if (result != ESP_OK) {
    release();
    return result;
  }

  audio_codec_i2c_cfg_t codecI2cConfig{};
  codecI2cConfig.port = static_cast<std::uint8_t>(config_.i2cPort);
  codecI2cConfig.addr = ES7210_CODEC_DEFAULT_ADDR;
  codecI2cConfig.bus_handle = i2cBus_;
  controlInterface_ = audio_codec_new_i2c_ctrl(&codecI2cConfig);
  if (controlInterface_ == nullptr) {
    release();
    return ESP_FAIL;
  }

  audio_codec_i2s_cfg_t codecI2sConfig{};
  codecI2sConfig.port = static_cast<std::uint8_t>(config_.i2sPort);
  codecI2sConfig.rx_handle = rxChannel_;
  dataInterface_ = audio_codec_new_i2s_data(&codecI2sConfig);
  if (dataInterface_ == nullptr) {
    release();
    return ESP_FAIL;
  }

  es7210_codec_cfg_t es7210Config{};
  es7210Config.ctrl_if =
      static_cast<const audio_codec_ctrl_if_t*>(controlInterface_);
  es7210Config.master_mode = false;
  es7210Config.mic_selected = config_.microphoneMask;
  codecInterface_ = es7210_codec_new(&es7210Config);
  if (codecInterface_ == nullptr) {
    release();
    return ESP_FAIL;
  }

  esp_codec_dev_cfg_t deviceConfig{};
  deviceConfig.codec_if =
      static_cast<const audio_codec_if_t*>(codecInterface_);
  deviceConfig.data_if = static_cast<const audio_codec_data_if_t*>(dataInterface_);
  deviceConfig.dev_type = ESP_CODEC_DEV_TYPE_IN;
  codecDevice_ = esp_codec_dev_new(&deviceConfig);
  if (codecDevice_ == nullptr) {
    release();
    return ESP_FAIL;
  }

  esp_codec_dev_sample_info_t sampleInfo{};
  sampleInfo.sample_rate = voice::kVoiceSampleRateHz;
  sampleInfo.channel = 2;
  sampleInfo.bits_per_sample = 32;
  result = esp_codec_dev_open(codecDevice_, &sampleInfo);
  if (result != ESP_CODEC_DEV_OK) {
    release();
    return ESP_FAIL;
  }
  result = esp_codec_dev_set_in_gain(codecDevice_, config_.inputGainDb);
  if (result != ESP_CODEC_DEV_OK) {
    release();
    return ESP_FAIL;
  }
  started_ = true;
  ESP_LOGI(kTag, "ES7210/I2S1 initialized (dual-MEMS, 32-bit, %.0f dB, layout-pick)",
           static_cast<double>(config_.inputGainDb));
  return ESP_OK;
}

void TEmbedMicrophone::run() noexcept {
  std::array<std::uint8_t, kDmaBytesPerRead> bytes{};
  std::array<std::int32_t, kDmaBytesPerRead / 2> slots{};
  bool channelEnabled = false;
  auto* rx = reinterpret_cast<i2s_chan_handle_t>(rxChannel_);
  while (started_) {
    if (capture_.state() == voice::VoiceCaptureState::Starting) {
      normalizer_.reset();
      voice::VoiceLayoutChoice choice{};
      std::size_t primedBytes = 0;
      if (!primeAndPickLayout(rx, channelEnabled, bytes.data(), bytes.size(),
                              config_.readTimeoutMs, &choice, &primedBytes)) {
        ESP_LOGW(kTag, "layout pick failed; retrying");
        vTaskDelay(pdMS_TO_TICKS(20));
        continue;
      }
      layout_ = choice.layout;
      normalizer_.setAlignment(voice::VoiceDmaAlignment::LeastSignificant);
      normalizer_.setChannelMode(choice.channel);
      ESP_LOGI(kTag,
               "capture layout=%s channel=%s score=%.2f rms=%.4f hf=%.2f ac1=%.2f",
               layoutName(layout_),
               choice.channel == voice::VoiceDmaChannelMode::Second ? "R"
                                                                    : "L",
               static_cast<double>(choice.quality.score),
               static_cast<double>(choice.quality.rms),
               static_cast<double>(choice.quality.hfRatio),
               static_cast<double>(choice.quality.ac1));
      if (!capture_.notifyStarted(voice::kVoicePcmFormat)) {
        ESP_LOGW(kTag, "notifyStarted rejected; retrying");
        vTaskDelay(pdMS_TO_TICKS(20));
        continue;
      }
      normalizer_.reset();
      (void)primedBytes;
      (void)kResyncAttempts;
    }
    if (capture_.state() == voice::VoiceCaptureState::Error) {
      if (channelEnabled) {
        (void)i2s_channel_disable(rx);
        channelEnabled = false;
      }
      normalizer_.reset();
      capture_.setAvailable(true);
      vTaskDelay(pdMS_TO_TICKS(20));
      continue;
    }
    if (capture_.state() != voice::VoiceCaptureState::Running) {
      if (channelEnabled) {
        (void)i2s_channel_disable(rx);
        channelEnabled = false;
      }
      normalizer_.reset();
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    std::size_t bytesRead = 0;
    if (!readRawWindow(rx, bytes.data(), bytes.size(), config_.readTimeoutMs,
                       &bytesRead)) {
      normalizer_.notifyTimeout();
    } else {
      const std::size_t slotCount = voice::expandVoiceDmaSlots(
          bytes.data(), bytesRead, layout_, slots.data(), slots.size());
      if (slotCount == 0 || slotCount % 2 != 0) {
        ESP_LOGW(kTag, "expand failed (%u bytes, layout=%s); resync",
                 static_cast<unsigned>(bytesRead), layoutName(layout_));
        normalizer_.reset();
        voice::VoiceLayoutChoice choice{};
        if (primeAndPickLayout(rx, channelEnabled, bytes.data(), bytes.size(),
                               config_.readTimeoutMs, &choice, nullptr)) {
          layout_ = choice.layout;
          normalizer_.setChannelMode(choice.channel);
          normalizer_.reset();
        } else {
          normalizer_.notifyReadError();
        }
      } else {
        normalizer_.ingest(slots.data(), slotCount);
      }
    }
  }
}

void TEmbedMicrophone::stop() noexcept {
  started_ = false;
  release();
}

void TEmbedMicrophone::release() noexcept {
  if (codecDevice_ != nullptr) {
    esp_codec_dev_close(codecDevice_);
    esp_codec_dev_delete(codecDevice_);
    codecDevice_ = nullptr;
  }
  if (codecInterface_ != nullptr) {
    audio_codec_delete_codec_if(
        static_cast<const audio_codec_if_t*>(codecInterface_));
    codecInterface_ = nullptr;
  }
  if (dataInterface_ != nullptr) {
    audio_codec_delete_data_if(
        static_cast<const audio_codec_data_if_t*>(dataInterface_));
    dataInterface_ = nullptr;
  }
  if (controlInterface_ != nullptr) {
    audio_codec_delete_ctrl_if(
        static_cast<const audio_codec_ctrl_if_t*>(controlInterface_));
    controlInterface_ = nullptr;
  }
  if (rxChannel_ != nullptr) {
    i2s_channel_disable(reinterpret_cast<i2s_chan_handle_t>(rxChannel_));
    i2s_del_channel(reinterpret_cast<i2s_chan_handle_t>(rxChannel_));
    rxChannel_ = nullptr;
  }
  if (i2cBus_ != nullptr) {
    i2c_del_master_bus(reinterpret_cast<i2c_master_bus_handle_t>(i2cBus_));
    i2cBus_ = nullptr;
  }
  started_ = false;
}

}  // namespace cadenza::idf
