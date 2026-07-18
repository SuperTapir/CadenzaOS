#include "t_embed_input.h"

#include "driver/gpio.h"
#include "esp_timer.h"

namespace cadenza::idf {
namespace {
constexpr gpio_num_t kEncoderA = GPIO_NUM_2;
constexpr gpio_num_t kEncoderB = GPIO_NUM_1;
constexpr gpio_num_t kEncoderButton = GPIO_NUM_0;
}  // namespace

esp_err_t TEmbedInput::start() noexcept {
  gpio_config_t input{};
  input.pin_bit_mask = (1ULL << kEncoderA) | (1ULL << kEncoderB) |
                       (1ULL << kEncoderButton);
  input.mode = GPIO_MODE_INPUT;
  input.pull_up_en = GPIO_PULLUP_ENABLE;
  esp_err_t result = gpio_config(&input);
  if (result != ESP_OK) return result;
  lastEncoderA_ = gpio_get_level(kEncoderA);
  lastButtonDown_ = gpio_get_level(kEncoderButton) == 0;
  if (lastButtonDown_) enqueue(RawInputType::ButtonDown, 0, nowMs());
  return ESP_OK;
}

void TEmbedInput::sample() noexcept {
  const auto now = nowMs();
  const int encoderA = gpio_get_level(kEncoderA);
  if (encoderA != lastEncoderA_ && encoderA == 0) {
    enqueue(RawInputType::Turn,
            gpio_get_level(kEncoderB) != encoderA ? 1 : -1, now);
  }
  lastEncoderA_ = encoderA;

  const bool buttonDown = gpio_get_level(kEncoderButton) == 0;
  if (buttonDown != lastButtonDown_) {
    lastButtonDown_ = buttonDown;
    enqueue(buttonDown ? RawInputType::ButtonDown : RawInputType::ButtonUp, 0,
            now);
  }

  while (read_ != write_) {
    reducer_.push(queue_[read_]);
    read_ = (read_ + 1) % kCapacity;
  }
}

InputFrame TEmbedInput::takeFrame() noexcept {
  reducer_.advanceTo(nowMs());
  return reducer_.takeFrame();
}

bool TEmbedInput::enqueue(RawInputType type, std::int32_t value,
                          MonotonicMillis timestamp) noexcept {
  const std::size_t next = (write_ + 1) % kCapacity;
  if (next == read_) return false;
  queue_[write_] = {type, timestamp, value};
  write_ = next;
  return true;
}

MonotonicMillis TEmbedInput::nowMs() noexcept {
  return static_cast<MonotonicMillis>(esp_timer_get_time() / 1000);
}

}  // namespace cadenza::idf
