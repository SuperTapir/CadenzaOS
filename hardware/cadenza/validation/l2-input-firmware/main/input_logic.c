#include "input_logic.h"

#include <assert.h>
#include <stddef.h>

/*
 * State bits are A:B. Positive sequence:
 *     00 -> 01 -> 11 -> 10 -> 00
 * The physical clockwise direction is deliberately not asserted here.
 * 2-bit jumps are invalid; repeated states are harmless no-ops.
 */
static const int8_t s_gray_delta[16] = {
    0, +1, -1, 0,
    -1, 0, 0, +1,
    +1, 0, 0, -1,
    0, -1, +1, 0,
};

void encoder_decoder_init(encoder_decoder_t *decoder, uint8_t initial_state)
{
    assert(decoder != NULL);
    *decoder = (encoder_decoder_t){
        .state = initial_state & 0x03U,
    };
}

int encoder_decoder_process(encoder_decoder_t *decoder, uint8_t new_state,
                            unsigned counts_per_detent, bool reverse)
{
    assert(decoder != NULL);
    assert(counts_per_detent >= 1U && counts_per_detent <= 4U);

    new_state &= 0x03U;
    const uint8_t old_state = decoder->state;
    if (new_state == old_state) {
        return 0;
    }

    const uint8_t transition = (uint8_t)((old_state << 2) | new_state);
    const int8_t delta = s_gray_delta[transition];
    decoder->state = new_state;

    if (delta == 0) {
        /* Resynchronise without inventing motion after a missed/illegal edge. */
        decoder->invalid_transitions++;
        decoder->partial_count = 0;
        return 0;
    }

    decoder->legal_transitions++;
    decoder->partial_count += delta;

    int event = 0;
    if (decoder->partial_count >= (int)counts_per_detent) {
        event = +1;
        decoder->partial_count -= (int8_t)counts_per_detent;
    } else if (decoder->partial_count <= -(int)counts_per_detent) {
        event = -1;
        decoder->partial_count += (int8_t)counts_per_detent;
    }

    if (reverse) {
        event = -event;
    }
    decoder->detent_count += event;
    return event;
}

void button_debounce_init(button_debounce_t *button, bool initial_pressed,
                          uint64_t now_us)
{
    assert(button != NULL);
    *button = (button_debounce_t){
        .raw_pressed = initial_pressed,
        .stable_pressed = initial_pressed,
        .long_reported = false,
        .raw_changed_us = now_us,
        .pressed_us = initial_pressed ? now_us : 0,
    };
}

unsigned button_debounce_sample(button_debounce_t *button, bool raw_pressed,
                                uint64_t now_us, uint32_t debounce_ms,
                                uint32_t long_press_ms)
{
    assert(button != NULL);
    unsigned events = BUTTON_EVENT_NONE;

    if (raw_pressed != button->raw_pressed) {
        button->raw_pressed = raw_pressed;
        button->raw_changed_us = now_us;
    }

    const uint64_t debounce_us = (uint64_t)debounce_ms * 1000U;
    if (button->raw_pressed != button->stable_pressed &&
        now_us - button->raw_changed_us >= debounce_us) {
        button->stable_pressed = button->raw_pressed;
        if (button->stable_pressed) {
            button->pressed_us = now_us;
            button->long_reported = false;
            events |= BUTTON_EVENT_PRESS;
        } else {
            events |= BUTTON_EVENT_RELEASE;
            if (!button->long_reported) {
                events |= BUTTON_EVENT_SHORT;
            }
        }
    }

    const uint64_t long_us = (uint64_t)long_press_ms * 1000U;
    if (button->stable_pressed && !button->long_reported &&
        now_us - button->pressed_us >= long_us) {
        button->long_reported = true;
        events |= BUTTON_EVENT_LONG;
    }

    return events;
}
