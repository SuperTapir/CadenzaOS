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
constexpr std::size_t kDmaSlotsPerRead = 256;

voice::VoiceDmaNormalizerConfig normalizerConfig() noexcept {
  voice::VoiceDmaNormalizerConfig config;
  config.input.sampleRateHz = voice::kVoiceSampleRateHz;
  config.input.validBits = 32;
  config.input.channels = 2;
  config.input.alignment = voice::VoiceDmaAlignment::MostSignificant;
  config.input.channelMode = voice::VoiceDmaChannelMode::Average;
  config.maxConsecutiveReadFailures = 3;
  return config;
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
  ESP_LOGI(kTag, "ES7210/I2S1 initialized (hardware validation pending)");
  return ESP_OK;
}

void TEmbedMicrophone::run() noexcept {
  std::array<std::int32_t, kDmaSlotsPerRead> slots{};
  bool channelEnabled = true;
  while (started_) {
    if (capture_.state() == voice::VoiceCaptureState::Starting) {
      normalizer_.reset();
      if (channelEnabled) {
        i2s_channel_disable(
            reinterpret_cast<i2s_chan_handle_t>(rxChannel_));
        channelEnabled = false;
      }
      const esp_err_t enabled = i2s_channel_enable(
          reinterpret_cast<i2s_chan_handle_t>(rxChannel_));
      if (enabled != ESP_OK ||
          !capture_.notifyStarted(voice::kVoicePcmFormat)) {
        capture_.notifyError();
        continue;
      }
      channelEnabled = true;
      // Drop the first DMA window after enable; partial/stale framing here is
      // what turns a USB replug into sticky harsh audio until the next restart.
      std::size_t primeBytes = 0;
      (void)i2s_channel_read(reinterpret_cast<i2s_chan_handle_t>(rxChannel_),
                             slots.data(), sizeof(slots), &primeBytes,
                             config_.readTimeoutMs);
      normalizer_.reset();
    }
    if (capture_.state() != voice::VoiceCaptureState::Running) {
      if (channelEnabled) {
        i2s_channel_disable(
            reinterpret_cast<i2s_chan_handle_t>(rxChannel_));
        channelEnabled = false;
      }
      normalizer_.reset();
      vTaskDelay(pdMS_TO_TICKS(5));
      continue;
    }
    std::size_t bytesRead = 0;
    const esp_err_t result = i2s_channel_read(
        reinterpret_cast<i2s_chan_handle_t>(rxChannel_), slots.data(),
        sizeof(slots), &bytesRead, config_.readTimeoutMs);
    if (result == ESP_ERR_TIMEOUT) {
      normalizer_.notifyTimeout();
    } else if (result != ESP_OK || bytesRead % sizeof(slots[0]) != 0) {
      normalizer_.notifyReadError();
    } else {
      const std::size_t slotCount = bytesRead / sizeof(slots[0]);
      // Hardware stereo DMA must arrive in channel groups. An odd slot count
      // after a USB remount would permanently slip L/R pairing.
      if (slotCount % normalizerConfig().input.channels != 0) {
        normalizer_.notifyReadError();
      } else {
        normalizer_.ingest(slots.data(), slotCount);
      }
    }
    if (capture_.state() == voice::VoiceCaptureState::Error) {
      ESP_LOGE(kTag, "capture stopped after bounded DMA read failures");
      stop();
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
