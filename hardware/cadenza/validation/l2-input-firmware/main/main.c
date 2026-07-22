#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "input_logic.h"
#include "sdkconfig.h"

/* Kconfig bools are omitted from sdkconfig.h when disabled. */
#ifndef CONFIG_L2_ENCODER_REVERSE
#define CONFIG_L2_ENCODER_REVERSE 0
#endif
#ifndef CONFIG_L2_EVENT_LOGGING
#define CONFIG_L2_EVENT_LOGGING 0
#endif

#define GPIO_ENC_A GPIO_NUM_15
#define GPIO_ENC_B GPIO_NUM_16
#define GPIO_BUTTON_A GPIO_NUM_17
#define GPIO_BUTTON_B GPIO_NUM_5
#define GPIO_BUTTON_MENU GPIO_NUM_8

#define ENCODER_QUEUE_LENGTH 128
#define INPUT_SAMPLE_MS 1

typedef struct {
    uint8_t state;
} encoder_edge_t;

typedef struct {
    const char *name;
    gpio_num_t gpio;
    button_debounce_t debounce;
    uint32_t raw_edges;
    uint32_t presses;
    uint32_t releases;
    uint32_t short_presses;
    uint32_t long_presses;
} button_input_t;

static const char *TAG = "l2_input";
static QueueHandle_t s_encoder_queue;
static SemaphoreHandle_t s_lock;
static portMUX_TYPE s_isr_stats_lock = portMUX_INITIALIZER_UNLOCKED;
static encoder_decoder_t s_encoder;
static button_input_t s_buttons[] = {
    {.name = "A(center)", .gpio = GPIO_BUTTON_A},
    {.name = "B", .gpio = GPIO_BUTTON_B},
    {.name = "Menu", .gpio = GPIO_BUTTON_MENU},
};
static bool s_reverse = CONFIG_L2_ENCODER_REVERSE;
static unsigned s_counts_per_detent = CONFIG_L2_ENCODER_COUNTS_PER_DETENT;
static bool s_event_logging = CONFIG_L2_EVENT_LOGGING;
static uint32_t s_encoder_a_edges;
static uint32_t s_encoder_b_edges;
static uint32_t s_encoder_queue_overflows;

static inline bool input_pressed(gpio_num_t gpio)
{
    return gpio_get_level(gpio) == 0;
}

static inline uint8_t encoder_state(void)
{
    return (uint8_t)((input_pressed(GPIO_ENC_A) ? 1U : 0U) << 1) |
           (uint8_t)(input_pressed(GPIO_ENC_B) ? 1U : 0U);
}

static void encoder_gpio_isr(void *argument)
{
    const gpio_num_t source = (gpio_num_t)(intptr_t)argument;
    const encoder_edge_t edge = {.state = encoder_state()};
    BaseType_t task_woken = pdFALSE;
    const BaseType_t queued = xQueueSendFromISR(s_encoder_queue, &edge, &task_woken);

    portENTER_CRITICAL_ISR(&s_isr_stats_lock);
    if (source == GPIO_ENC_A) {
        s_encoder_a_edges++;
    } else {
        s_encoder_b_edges++;
    }
    if (queued != pdTRUE) {
        s_encoder_queue_overflows++;
    }
    portEXIT_CRITICAL_ISR(&s_isr_stats_lock);
    if (task_woken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void emit_button_events(button_input_t *button, unsigned events)
{
    if (!s_event_logging) {
        if (events & BUTTON_EVENT_PRESS) {
            button->presses++;
        }
        if (events & BUTTON_EVENT_LONG) {
            button->long_presses++;
        }
        if (events & BUTTON_EVENT_RELEASE) {
            button->releases++;
        }
        if (events & BUTTON_EVENT_SHORT) {
            button->short_presses++;
        }
        return;
    }
    if (events & BUTTON_EVENT_PRESS) {
        button->presses++;
        printf("EVENT button=%s type=PRESS\n", button->name);
    }
    if (events & BUTTON_EVENT_LONG) {
        button->long_presses++;
        printf("EVENT button=%s type=LONG threshold_ms=%d\n", button->name,
               CONFIG_L2_BUTTON_LONG_PRESS_MS);
    }
    if (events & BUTTON_EVENT_RELEASE) {
        button->releases++;
        printf("EVENT button=%s type=RELEASE\n", button->name);
    }
    if (events & BUTTON_EVENT_SHORT) {
        button->short_presses++;
        printf("EVENT button=%s type=SHORT\n", button->name);
    }
}

static void print_status_locked(void)
{
    const bool raw_a = input_pressed(GPIO_ENC_A);
    const bool raw_b = input_pressed(GPIO_ENC_B);
    uint32_t encoder_a_edges;
    uint32_t encoder_b_edges;
    uint32_t encoder_queue_overflows;
    portENTER_CRITICAL(&s_isr_stats_lock);
    encoder_a_edges = s_encoder_a_edges;
    encoder_b_edges = s_encoder_b_edges;
    encoder_queue_overflows = s_encoder_queue_overflows;
    portEXIT_CRITICAL(&s_isr_stats_lock);
    printf("STATUS enc_raw=%u%u enc_stable=%u%u partial=%d detents=%" PRId32
           " legal=%" PRIu32 " invalid=%" PRIu32 " edges_a=%" PRIu32
           " edges_b=%" PRIu32 " queue_overflow=%" PRIu32
           " reverse=%u cpd=%u\n",
           raw_a, raw_b, (s_encoder.state >> 1) & 1U, s_encoder.state & 1U,
           s_encoder.partial_count, s_encoder.detent_count,
           s_encoder.legal_transitions, s_encoder.invalid_transitions,
           encoder_a_edges, encoder_b_edges, encoder_queue_overflows,
           s_reverse, s_counts_per_detent);
    for (size_t i = 0; i < sizeof(s_buttons) / sizeof(s_buttons[0]); ++i) {
        const button_input_t *button = &s_buttons[i];
        printf("STATUS button=%s gpio=%d raw=%u stable=%u raw_edges=%" PRIu32
               " presses=%" PRIu32 " releases=%" PRIu32 " short=%" PRIu32
               " long=%" PRIu32 "\n",
               button->name, button->gpio, button->debounce.raw_pressed,
               button->debounce.stable_pressed, button->raw_edges,
               button->presses, button->releases, button->short_presses,
               button->long_presses);
    }
}

static void input_task(void *unused)
{
    (void)unused;
    int64_t next_sample_us = esp_timer_get_time();
    int64_t next_status_us = next_sample_us + (int64_t)CONFIG_L2_STATUS_PERIOD_MS * 1000;

    while (true) {
        encoder_edge_t edge;
        if (xQueueReceive(s_encoder_queue, &edge, pdMS_TO_TICKS(INPUT_SAMPLE_MS)) == pdTRUE) {
            do {
                xSemaphoreTake(s_lock, portMAX_DELAY);
                const int movement = encoder_decoder_process(
                    &s_encoder, edge.state, s_counts_per_detent, s_reverse);
                if (movement != 0 && s_event_logging) {
                    printf("EVENT encoder=%s detents=%" PRId32 "\n",
                           movement > 0 ? "+1" : "-1", s_encoder.detent_count);
                }
                xSemaphoreGive(s_lock);
            } while (xQueueReceive(s_encoder_queue, &edge, 0) == pdTRUE);
        }

        const int64_t now_us = esp_timer_get_time();
        if (now_us >= next_sample_us) {
            xSemaphoreTake(s_lock, portMAX_DELAY);
            for (size_t i = 0; i < sizeof(s_buttons) / sizeof(s_buttons[0]); ++i) {
                button_input_t *button = &s_buttons[i];
                const bool old_raw = button->debounce.raw_pressed;
                const bool raw = input_pressed(button->gpio);
                const unsigned events = button_debounce_sample(
                    &button->debounce, raw, (uint64_t)now_us,
                    CONFIG_L2_BUTTON_DEBOUNCE_MS,
                    CONFIG_L2_BUTTON_LONG_PRESS_MS);
                if (raw != old_raw) {
                    button->raw_edges++;
                }
                emit_button_events(button, events);
            }
            xSemaphoreGive(s_lock);
            next_sample_us = now_us + INPUT_SAMPLE_MS * 1000;
        }

        if (now_us >= next_status_us) {
            xSemaphoreTake(s_lock, portMAX_DELAY);
            print_status_locked();
            xSemaphoreGive(s_lock);
            next_status_us = now_us + (int64_t)CONFIG_L2_STATUS_PERIOD_MS * 1000;
        }
    }
}

static void reset_statistics_locked(void)
{
    const uint8_t state = encoder_state();
    encoder_decoder_init(&s_encoder, state);
    portENTER_CRITICAL(&s_isr_stats_lock);
    s_encoder_a_edges = 0;
    s_encoder_b_edges = 0;
    s_encoder_queue_overflows = 0;
    portEXIT_CRITICAL(&s_isr_stats_lock);
    for (size_t i = 0; i < sizeof(s_buttons) / sizeof(s_buttons[0]); ++i) {
        s_buttons[i].raw_edges = 0;
        s_buttons[i].presses = 0;
        s_buttons[i].releases = 0;
        s_buttons[i].short_presses = 0;
        s_buttons[i].long_presses = 0;
    }
}

static void print_help(void)
{
    puts("Commands: status | reset | reverse 0|1 | cpd 1..4 | events 0|1 | help");
    puts("Direction labels are logical only until the real EC12 A/B order is measured.");
}

static void console_task(void *unused)
{
    (void)unused;
    char line[64];
    print_help();
    while (fgets(line, sizeof(line), stdin) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        xSemaphoreTake(s_lock, portMAX_DELAY);
        if (strcmp(line, "status") == 0) {
            print_status_locked();
        } else if (strcmp(line, "reset") == 0) {
            reset_statistics_locked();
            puts("OK statistics reset");
        } else if (strncmp(line, "reverse ", 8) == 0 &&
                   (line[8] == '0' || line[8] == '1') && line[9] == '\0') {
            s_reverse = line[8] == '1';
            s_encoder.partial_count = 0;
            printf("OK reverse=%u\n", s_reverse);
        } else if (strncmp(line, "cpd ", 4) == 0) {
            char *end = NULL;
            const long value = strtol(line + 4, &end, 10);
            if (end != line + 4 && *end == '\0' && value >= 1 && value <= 4) {
                s_counts_per_detent = (unsigned)value;
                s_encoder.partial_count = 0;
                printf("OK cpd=%u\n", s_counts_per_detent);
            } else {
                puts("ERR cpd must be 1..4");
            }
        } else if (strncmp(line, "events ", 7) == 0 &&
                   (line[7] == '0' || line[7] == '1') && line[8] == '\0') {
            s_event_logging = line[7] == '1';
            printf("OK events=%u\n", s_event_logging);
        } else if (strcmp(line, "help") == 0) {
            print_help();
        } else if (line[0] != '\0') {
            puts("ERR unknown command");
            print_help();
        }
        xSemaphoreGive(s_lock);
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    const uint64_t input_mask = (1ULL << GPIO_ENC_A) | (1ULL << GPIO_ENC_B) |
                                (1ULL << GPIO_BUTTON_A) | (1ULL << GPIO_BUTTON_B) |
                                (1ULL << GPIO_BUTTON_MENU);
    const gpio_config_t input_gpio_config = {
        .pin_bit_mask = input_mask,
        .mode = GPIO_MODE_INPUT,
        /* External 10 kOhm pull-ups are required; internal pull-ups are backup only. */
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&input_gpio_config));

    s_lock = xSemaphoreCreateMutex();
    s_encoder_queue = xQueueCreate(ENCODER_QUEUE_LENGTH, sizeof(encoder_edge_t));
    if (s_lock == NULL || s_encoder_queue == NULL) {
        ESP_LOGE(TAG, "Failed to allocate synchronization objects");
        abort();
    }

    const uint64_t now_us = (uint64_t)esp_timer_get_time();
    encoder_decoder_init(&s_encoder, encoder_state());
    for (size_t i = 0; i < sizeof(s_buttons) / sizeof(s_buttons[0]); ++i) {
        button_debounce_init(&s_buttons[i].debounce,
                             input_pressed(s_buttons[i].gpio), now_us);
    }

    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_set_intr_type(GPIO_ENC_A, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_set_intr_type(GPIO_ENC_B, GPIO_INTR_ANYEDGE));
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_ENC_A, encoder_gpio_isr,
                                         (void *)(intptr_t)GPIO_ENC_A));
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_ENC_B, encoder_gpio_isr,
                                         (void *)(intptr_t)GPIO_ENC_B));

    printf("\nCadenza L2 input validation only; active-low; external 10k pull-ups assumed.\n");
    printf("ENC_A=GPIO15 ENC_B=GPIO16 A(center)=GPIO17 B=GPIO5 Menu=GPIO8\n");
    printf("debounce=%dms long=%dms reverse=%u counts_per_detent=%u events=%u\n",
           CONFIG_L2_BUTTON_DEBOUNCE_MS, CONFIG_L2_BUTTON_LONG_PRESS_MS,
           s_reverse, s_counts_per_detent, s_event_logging);
    puts("EC12 pinout, physical direction, and pulses/detent are NOT assumed.");

    if (xTaskCreate(input_task, "l2_input", 4096, NULL, 12, NULL) != pdPASS ||
        xTaskCreate(console_task, "l2_console", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create input validation tasks");
        abort();
    }
}
