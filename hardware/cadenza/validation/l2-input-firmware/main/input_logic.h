#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t state;
    int8_t partial_count;
    int32_t detent_count;
    uint32_t legal_transitions;
    uint32_t invalid_transitions;
} encoder_decoder_t;

typedef struct {
    bool raw_pressed;
    bool stable_pressed;
    bool long_reported;
    uint64_t raw_changed_us;
    uint64_t pressed_us;
} button_debounce_t;

enum {
    BUTTON_EVENT_NONE = 0,
    BUTTON_EVENT_PRESS = 1U << 0,
    BUTTON_EVENT_RELEASE = 1U << 1,
    BUTTON_EVENT_SHORT = 1U << 2,
    BUTTON_EVENT_LONG = 1U << 3,
};

void encoder_decoder_init(encoder_decoder_t *decoder, uint8_t initial_state);
int encoder_decoder_process(encoder_decoder_t *decoder, uint8_t new_state,
                            unsigned counts_per_detent, bool reverse);

void button_debounce_init(button_debounce_t *button, bool initial_pressed,
                          uint64_t now_us);
unsigned button_debounce_sample(button_debounce_t *button, bool raw_pressed,
                                uint64_t now_us, uint32_t debounce_ms,
                                uint32_t long_press_ms);
