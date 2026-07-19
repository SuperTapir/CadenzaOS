#include "t_embed_input.h"

#include "driver/gpio.h"
#include "driver/pcnt.h"
#include "esp_timer.h"

namespace cadenza::idf {
namespace {

constexpr gpio_num_t kEncoderA = GPIO_NUM_2;
constexpr gpio_num_t kEncoderB = GPIO_NUM_1;
constexpr gpio_num_t kEncoderButton = GPIO_NUM_0;
constexpr pcnt_unit_t kEncoderUnit = PCNT_UNIT_0;
// Positive matches desktop Right / mouse-wheel "forward" navigation.
constexpr std::int32_t kEncoderSign = 1;

bool configureEncoderPcnt() {
  pcnt_config_t channel = {};
  channel.pulse_gpio_num = kEncoderA;
  channel.ctrl_gpio_num = kEncoderB;
  channel.channel = PCNT_CHANNEL_0;
  channel.unit = kEncoderUnit;
  channel.pos_mode = PCNT_COUNT_DEC;
  channel.neg_mode = PCNT_COUNT_INC;
  channel.lctrl_mode = PCNT_MODE_REVERSE;
  channel.hctrl_mode = PCNT_MODE_KEEP;
  channel.counter_h_lim = 30000;
  channel.counter_l_lim = -30000;
  if (pcnt_unit_config(&channel) != ESP_OK) return false;

  channel.pulse_gpio_num = kEncoderB;
  channel.ctrl_gpio_num = kEncoderA;
  channel.channel = PCNT_CHANNEL_1;
  channel.pos_mode = PCNT_COUNT_INC;
  channel.neg_mode = PCNT_COUNT_DEC;
  if (pcnt_unit_config(&channel) != ESP_OK) return false;

  if (pcnt_set_filter_value(kEncoderUnit, 1023) != ESP_OK) return false;
  if (pcnt_filter_enable(kEncoderUnit) != ESP_OK) return false;
  if (pcnt_counter_pause(kEncoderUnit) != ESP_OK) return false;
  if (pcnt_counter_clear(kEncoderUnit) != ESP_OK) return false;
  if (pcnt_counter_resume(kEncoderUnit) != ESP_OK) return false;
  return true;
}

}  // namespace

esp_err_t TEmbedInput::start() noexcept {
  gpio_config_t input{};
  input.pin_bit_mask = (1ULL << kEncoderA) | (1ULL << kEncoderB) |
                       (1ULL << kEncoderButton);
  input.mode = GPIO_MODE_INPUT;
  input.pull_up_en = GPIO_PULLUP_ENABLE;
  esp_err_t result = gpio_config(&input);
  if (result != ESP_OK) return result;
  gpio_pullup_en(kEncoderA);
  gpio_pullup_en(kEncoderB);

  encoderReady_ = configureEncoderPcnt();
  lastEncoderCount_ = 0;
  encoderStepResidue_ = 0;
  if (encoderReady_) {
    std::int16_t count = 0;
    pcnt_get_counter_value(kEncoderUnit, &count);
    lastEncoderCount_ = count;
  }

  lastButtonDown_ = gpio_get_level(kEncoderButton) == 0;
  if (lastButtonDown_) enqueue(RawInputType::ButtonDown, 0, nowMs());
  return encoderReady_ ? ESP_OK : ESP_FAIL;
}

void TEmbedInput::drainEncoder(MonotonicMillis timestamp) noexcept {
  if (!encoderReady_) return;

  std::int16_t count = 0;
  if (pcnt_get_counter_value(kEncoderUnit, &count) != ESP_OK) return;
  const std::int32_t delta =
      kEncoderSign * (static_cast<std::int32_t>(count) - lastEncoderCount_);
  lastEncoderCount_ = count;
  if (delta == 0) return;

  encoderStepResidue_ += delta;
  while (encoderStepResidue_ >= kStepsPerDetent) {
    enqueue(RawInputType::Turn, 1, timestamp);
    encoderStepResidue_ -= kStepsPerDetent;
  }
  while (encoderStepResidue_ <= -kStepsPerDetent) {
    enqueue(RawInputType::Turn, -1, timestamp);
    encoderStepResidue_ += kStepsPerDetent;
  }
}

void TEmbedInput::sample() noexcept {
  const auto now = nowMs();
  drainEncoder(now);

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
  sample();
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
