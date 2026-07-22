#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "../main/input_logic.h"

static void test_encoder_both_directions(void)
{
    encoder_decoder_t decoder;
    encoder_decoder_init(&decoder, 0);
    const uint8_t positive[] = {1, 3, 2, 0};
    int events = 0;
    for (unsigned i = 0; i < sizeof(positive); ++i) {
        events += encoder_decoder_process(&decoder, positive[i], 4, false);
    }
    assert(events == 1);
    assert(decoder.detent_count == 1);

    const uint8_t negative[] = {2, 3, 1, 0};
    events = 0;
    for (unsigned i = 0; i < sizeof(negative); ++i) {
        events += encoder_decoder_process(&decoder, negative[i], 4, false);
    }
    assert(events == -1);
    assert(decoder.detent_count == 0);
}

static void test_encoder_bounce_invalid_and_configuration(void)
{
    encoder_decoder_t decoder;
    encoder_decoder_init(&decoder, 0);
    const uint8_t bounce_then_forward[] = {1, 0, 1, 3, 2, 0};
    int events = 0;
    for (unsigned i = 0; i < sizeof(bounce_then_forward); ++i) {
        events += encoder_decoder_process(&decoder, bounce_then_forward[i], 4, false);
    }
    assert(events == 1);

    encoder_decoder_init(&decoder, 0);
    assert(encoder_decoder_process(&decoder, 3, 4, false) == 0);
    assert(decoder.invalid_transitions == 1);
    assert(decoder.partial_count == 0);
    const uint8_t recovered[] = {2, 0, 1, 3};
    events = 0;
    for (unsigned i = 0; i < sizeof(recovered); ++i) {
        events += encoder_decoder_process(&decoder, recovered[i], 4, false);
    }
    assert(events == 1);
    assert(decoder.detent_count == 1);

    encoder_decoder_init(&decoder, 0);
    assert(encoder_decoder_process(&decoder, 1, 2, false) == 0);
    assert(encoder_decoder_process(&decoder, 3, 2, false) == 1);
    assert(decoder.detent_count == 1);

    encoder_decoder_init(&decoder, 0);
    assert(encoder_decoder_process(&decoder, 1, 1, true) == -1);
    assert(decoder.detent_count == -1);
}

static void test_button_debounce_short_and_long(void)
{
    button_debounce_t button;
    button_debounce_init(&button, false, 0);
    assert(button_debounce_sample(&button, true, 1000, 20, 800) == 0);
    assert(button_debounce_sample(&button, false, 10000, 20, 800) == 0);
    assert(button_debounce_sample(&button, true, 20000, 20, 800) == 0);
    assert(button_debounce_sample(&button, true, 40000, 20, 800) == BUTTON_EVENT_PRESS);
    assert(button_debounce_sample(&button, false, 100000, 20, 800) == 0);
    assert(button_debounce_sample(&button, false, 120000, 20, 800) ==
           (BUTTON_EVENT_RELEASE | BUTTON_EVENT_SHORT));

    assert(button_debounce_sample(&button, true, 200000, 20, 800) == 0);
    assert(button_debounce_sample(&button, true, 220000, 20, 800) == BUTTON_EVENT_PRESS);
    assert(button_debounce_sample(&button, true, 1020000, 20, 800) == BUTTON_EVENT_LONG);
    assert(button_debounce_sample(&button, true, 1100000, 20, 800) == 0);
    assert(button_debounce_sample(&button, false, 1200000, 20, 800) == 0);
    assert(button_debounce_sample(&button, false, 1220000, 20, 800) == BUTTON_EVENT_RELEASE);
    assert(button.long_reported);
}

int main(void)
{
    test_encoder_both_directions();
    test_encoder_bounce_invalid_and_configuration();
    test_button_debounce_short_and_long();
    puts("PASS: input logic host tests");
    return 0;
}
