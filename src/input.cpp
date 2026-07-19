#include "input.h"

#include "board_pins.h"
#include "driver/gpio.h"
#include "driver/pcnt.h"

namespace {
// Hardware PCNT full-quadrature decode (Espressif rotary_encoder example +
// ESP32Encoder community practice). ISR-free counting survives slow present
// frames; glitch filter absorbs EC11 contact bounce.
constexpr pcnt_unit_t kEncoderUnit = PCNT_UNIT_0;
// Sign flips CW/CCW to match desktop Right = +1. Invert here if A/B wiring
// relative to the Espressif channel recipe yields the opposite feel.
constexpr std::int32_t kEncoderSign = 1;

bool configureEncoderPcnt() {
  // Channel 0: A = pulse, B = control (Espressif EC11 recipe).
  pcnt_config_t channel = {};
  channel.pulse_gpio_num = BoardPins::kEncoderA;
  channel.ctrl_gpio_num = BoardPins::kEncoderB;
  channel.channel = PCNT_CHANNEL_0;
  channel.unit = kEncoderUnit;
  channel.pos_mode = PCNT_COUNT_DEC;
  channel.neg_mode = PCNT_COUNT_INC;
  channel.lctrl_mode = PCNT_MODE_REVERSE;
  channel.hctrl_mode = PCNT_MODE_KEEP;
  channel.counter_h_lim = 30000;
  channel.counter_l_lim = -30000;
  if (pcnt_unit_config(&channel) != ESP_OK) return false;

  // Channel 1: B = pulse, A = control — completes full quadrature.
  channel.pulse_gpio_num = BoardPins::kEncoderB;
  channel.ctrl_gpio_num = BoardPins::kEncoderA;
  channel.channel = PCNT_CHANNEL_1;
  channel.pos_mode = PCNT_COUNT_INC;
  channel.neg_mode = PCNT_COUNT_DEC;
  if (pcnt_unit_config(&channel) != ESP_OK) return false;

  // Max hardware filter (~12 µs @ 80 MHz APB). Same ceiling ESP32Encoder uses
  // for KY-040 / EC11 mechanical bounce without dropping human-speed detents.
  if (pcnt_set_filter_value(kEncoderUnit, 1023) != ESP_OK) return false;
  if (pcnt_filter_enable(kEncoderUnit) != ESP_OK) return false;
  if (pcnt_counter_pause(kEncoderUnit) != ESP_OK) return false;
  if (pcnt_counter_clear(kEncoderUnit) != ESP_OK) return false;
  if (pcnt_counter_resume(kEncoderUnit) != ESP_OK) return false;
  return true;
}
}  // namespace

void TEmbedRawInputSource::begin() {
  pinMode(BoardPins::kEncoderA, INPUT_PULLUP);
  pinMode(BoardPins::kEncoderB, INPUT_PULLUP);
  pinMode(BoardPins::kEncoderButton, INPUT_PULLUP);
  gpio_pullup_en(static_cast<gpio_num_t>(BoardPins::kEncoderA));
  gpio_pullup_en(static_cast<gpio_num_t>(BoardPins::kEncoderB));

  encoderReady_ = configureEncoderPcnt();
  lastEncoderCount_ = 0;
  encoderStepResidue_ = 0;
  if (encoderReady_) {
    std::int16_t count = 0;
    pcnt_get_counter_value(kEncoderUnit, &count);
    lastEncoderCount_ = count;
  }

  lastButtonDown_ = digitalRead(BoardPins::kEncoderButton) == LOW;
  if (lastButtonDown_) {
    enqueue(cadenza::RawInputType::ButtonDown, 0, nowMs());
  }
}

void TEmbedRawInputSource::drainEncoder(
    cadenza::MonotonicMillis timestampMs) noexcept {
  if (!encoderReady_) return;

  std::int16_t count = 0;
  if (pcnt_get_counter_value(kEncoderUnit, &count) != ESP_OK) return;
  const std::int32_t delta =
      kEncoderSign * (static_cast<std::int32_t>(count) - lastEncoderCount_);
  lastEncoderCount_ = count;
  if (delta == 0) return;

  encoderStepResidue_ += delta;
  while (encoderStepResidue_ >= kStepsPerDetent) {
    // Positive matches desktop Right / mouse-wheel "forward" navigation.
    enqueue(cadenza::RawInputType::Turn, 1, timestampMs);
    encoderStepResidue_ -= kStepsPerDetent;
  }
  while (encoderStepResidue_ <= -kStepsPerDetent) {
    enqueue(cadenza::RawInputType::Turn, -1, timestampMs);
    encoderStepResidue_ += kStepsPerDetent;
  }
}

void TEmbedRawInputSource::sample() {
  const auto now = nowMs();
  drainEncoder(now);

  const bool buttonDown = digitalRead(BoardPins::kEncoderButton) == LOW;
  if (buttonDown != lastButtonDown_) {
    lastButtonDown_ = buttonDown;
    enqueue(buttonDown ? cadenza::RawInputType::ButtonDown
                       : cadenza::RawInputType::ButtonUp,
            0, now);
  }
}

bool TEmbedRawInputSource::poll(cadenza::RawInputEvent& event) noexcept {
  if (readPosition_ == writePosition_) {
    return false;
  }
  event = queue_[readPosition_];
  readPosition_ = (readPosition_ + 1) % kQueueCapacity;
  return true;
}

cadenza::MonotonicMillis TEmbedRawInputSource::nowMs() const noexcept {
  return millis();
}

bool TEmbedRawInputSource::enqueue(cadenza::RawInputType type, int32_t value,
                                   cadenza::MonotonicMillis timestampMs) noexcept {
  const uint8_t next = (writePosition_ + 1) % kQueueCapacity;
  if (next == readPosition_) {
    return false;
  }
  queue_[writePosition_] = {type, timestampMs, value};
  writePosition_ = next;
  return true;
}

void InputController::begin() { source_.begin(); }

void InputController::sample() {
  source_.sample();
  cadenza::pumpInput(source_, source_, reducer_);
}

InputFrame InputController::takeFrame() {
  // Drain any edges that arrived during the previous present/render.
  sample();
  reducer_.advanceTo(source_.nowMs());
  return reducer_.takeFrame();
}
