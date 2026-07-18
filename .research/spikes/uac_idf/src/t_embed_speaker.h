#pragma once

#include <atomic>

#include "cadenza/audio/interaction_sound.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace cadenza::idf {

// ESP-IDF 5-only I2S0 owner. The output task is the sole consumer of the
// InteractionSoundService PCM port and owns its DMA channel and local buffers.
class TEmbedSpeaker {
 public:
  TEmbedSpeaker() noexcept = default;
  ~TEmbedSpeaker();

  TEmbedSpeaker(const TEmbedSpeaker&) = delete;
  TEmbedSpeaker& operator=(const TEmbedSpeaker&) = delete;

  esp_err_t start(audio::InteractionSoundService& service) noexcept;
  void stop() noexcept;
  bool running() const noexcept {
    return running_.load(std::memory_order_acquire);
  }

 private:
  static void taskEntry(void* context) noexcept;
  void run() noexcept;
  void releaseChannel() noexcept;

  audio::InteractionSoundService* service_ = nullptr;
  void* txChannel_ = nullptr;
  std::atomic<bool> running_{false};
  std::atomic<TaskHandle_t> task_{nullptr};
};

}  // namespace cadenza::idf
