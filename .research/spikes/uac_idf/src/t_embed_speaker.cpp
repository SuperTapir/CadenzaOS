#include "t_embed_speaker.h"

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/audio/i2s_audio_format.h"
#include "cadenza_t_embed_audio_contract.h"
#include "driver/i2s_std.h"
#include "esp_log.h"

namespace cadenza::idf {
namespace {

constexpr char kTag[] = "cadenza_spk";
constexpr std::uint32_t kWriteTimeoutMs = 20;

}  // namespace

TEmbedSpeaker::~TEmbedSpeaker() { stop(); }

esp_err_t TEmbedSpeaker::start(
    audio::InteractionSoundService& service) noexcept {
  if (running()) return ESP_OK;
  service_ = &service;

  i2s_chan_config_t channelConfig = I2S_CHANNEL_DEFAULT_CONFIG(
      static_cast<i2s_port_t>(t_embed_audio::kSpeaker.i2sPort),
      I2S_ROLE_MASTER);
  channelConfig.dma_desc_num = t_embed_audio::kSpeaker.dmaDescriptors;
  channelConfig.dma_frame_num = t_embed_audio::kSpeaker.dmaFrames;
  auto* tx = reinterpret_cast<i2s_chan_handle_t*>(&txChannel_);
  esp_err_t result = i2s_new_channel(&channelConfig, tx, nullptr);
  if (result != ESP_OK) {
    releaseChannel();
    return result;
  }

  i2s_std_config_t standardConfig{
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(
          t_embed_audio::kSpeaker.sampleRateHz),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
          I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
      .gpio_cfg =
          {
              .mclk = I2S_GPIO_UNUSED,
              .bclk = static_cast<gpio_num_t>(t_embed_audio::kSpeakerBclkPin),
              .ws = static_cast<gpio_num_t>(
                  t_embed_audio::kSpeakerWordSelectPin),
              .dout = static_cast<gpio_num_t>(
                  t_embed_audio::kSpeakerDataOutPin),
              .din = I2S_GPIO_UNUSED,
              .invert_flags = {},
          },
  };
  result = i2s_channel_init_std_mode(
      reinterpret_cast<i2s_chan_handle_t>(txChannel_), &standardConfig);
  if (result == ESP_OK) {
    result = i2s_channel_enable(
        reinterpret_cast<i2s_chan_handle_t>(txChannel_));
  }
  if (result != ESP_OK) {
    releaseChannel();
    return result;
  }

  running_.store(true, std::memory_order_release);
  TaskHandle_t createdTask = nullptr;
  const BaseType_t created = xTaskCreatePinnedToCore(
      taskEntry, "cadenza_spk", t_embed_audio::kSpeaker.taskStackBytes, this,
      t_embed_audio::kSpeaker.taskPriority, &createdTask,
      t_embed_audio::kSpeaker.taskCore);
  if (created != pdPASS) {
    running_.store(false, std::memory_order_release);
    releaseChannel();
    return ESP_ERR_NO_MEM;
  }
  task_.store(createdTask, std::memory_order_release);
  ESP_LOGI(kTag, "I2S0 output started (hardware validation pending)");
  return ESP_OK;
}

void TEmbedSpeaker::taskEntry(void* context) noexcept {
  static_cast<TEmbedSpeaker*>(context)->run();
}

void TEmbedSpeaker::run() noexcept {
  std::array<std::int16_t, t_embed_audio::kSpeaker.dmaFrames> mono{};
  std::array<audio::StereoI2sFrame, t_embed_audio::kSpeaker.dmaFrames> stereo{};
  while (running()) {
    service_->render(mono.data(), mono.size());
    audio::duplicateMonoToStereo(mono.data(), stereo.data(), mono.size());
    std::size_t bytesWritten = 0;
    const esp_err_t result = i2s_channel_write(
        reinterpret_cast<i2s_chan_handle_t>(txChannel_), stereo.data(),
        sizeof(stereo), &bytesWritten, kWriteTimeoutMs);
    if (result != ESP_OK || bytesWritten != sizeof(stereo)) {
      ESP_LOGE(kTag, "I2S0 write failed: err=%d bytes=%u", result,
               static_cast<unsigned>(bytesWritten));
      running_.store(false, std::memory_order_release);
    }
  }
  task_.store(nullptr, std::memory_order_release);
  vTaskDelete(nullptr);
}

void TEmbedSpeaker::stop() noexcept {
  running_.store(false, std::memory_order_release);
  for (std::uint8_t attempt = 0;
       task_.load(std::memory_order_acquire) != nullptr && attempt < 20;
       ++attempt) {
    vTaskDelay(pdMS_TO_TICKS(5));
  }
  releaseChannel();
  service_ = nullptr;
}

void TEmbedSpeaker::releaseChannel() noexcept {
  if (txChannel_ == nullptr) return;
  i2s_channel_disable(reinterpret_cast<i2s_chan_handle_t>(txChannel_));
  i2s_del_channel(reinterpret_cast<i2s_chan_handle_t>(txChannel_));
  txChannel_ = nullptr;
}

}  // namespace cadenza::idf
