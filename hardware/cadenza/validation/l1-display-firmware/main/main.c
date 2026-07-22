/*
 * Minimal LS027B7DH01 breadboard bring-up for ESP32-S3 / ESP-IDF 5.5.
 *
 * The serial protocol below follows Sharp LCY-1210401A sections 6-3 through
 * 6-6.  Its byte framing is cross-checked against u8g2's LS027B7DH01 driver
 * at commit ab9e48b2228351e9476682a70b7f3ee4909cd585.
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

#define LCD_WIDTH 400
#define LCD_HEIGHT 240
#define LCD_BYTES_PER_LINE (LCD_WIDTH / 8)
#define LCD_FRAME_WIRE_BYTES (1 + LCD_HEIGHT * (1 + LCD_BYTES_PER_LINE + 1) + 1)

#define LCD_GPIO_SCLK GPIO_NUM_39
#define LCD_GPIO_SI GPIO_NUM_48
#define LCD_GPIO_SCS GPIO_NUM_47
#define LCD_GPIO_EXTCOMIN GPIO_NUM_14
#define LCD_GPIO_DISP GPIO_NUM_12

#define LCD_SPI_CLOCK_HZ 1000000
#define LCD_EXTCOMIN_HZ 2
#define LCD_REFRESH_HZ 4
#define LCD_PATTERN_SECONDS 30

#define LCD_SPI_HOST SPI2_HOST
#define LCD_EXTCOMIN_TIMER LEDC_TIMER_0
#define LCD_EXTCOMIN_CHANNEL LEDC_CHANNEL_0
#define LCD_EXTCOMIN_MODE LEDC_LOW_SPEED_MODE

typedef enum {
    PATTERN_WHITE = 0,
    PATTERN_BLACK,
    PATTERN_VERTICAL_STRIPES,
    PATTERN_CHECKERBOARD,
    PATTERN_COUNT,
} lcd_pattern_t;

static const char *TAG = "l1_display";
static spi_device_handle_t s_spi;
static uint8_t s_wire_frame[LCD_FRAME_WIRE_BYTES];
static SemaphoreHandle_t s_spi_mutex;
static SemaphoreHandle_t s_state_mutex;
static lcd_pattern_t s_pattern = PATTERN_WHITE;
static bool s_refresh_enabled;
static bool s_auto_cycle;
static bool s_display_started;
static int64_t s_pattern_started_us;

_Static_assert(LCD_BYTES_PER_LINE == 50, "LS027B7DH01 must use 50 bytes per line");
_Static_assert(LCD_FRAME_WIRE_BYTES == 12482, "Unexpected full-frame wire size");

static const char *pattern_name(lcd_pattern_t pattern)
{
    switch (pattern) {
    case PATTERN_WHITE:
        return "全白 / white";
    case PATTERN_BLACK:
        return "全黑 / black";
    case PATTERN_VERTICAL_STRIPES:
        return "竖条 / vertical stripes";
    case PATTERN_CHECKERBOARD:
        return "棋盘格 / checkerboard";
    default:
        return "未知 / unknown";
    }
}

static uint8_t pattern_byte(lcd_pattern_t pattern, unsigned int line)
{
    switch (pattern) {
    case PATTERN_WHITE:
        /* u8g2 represents black ink with set bits and sends LS027 data unchanged. */
        return 0x00;
    case PATTERN_BLACK:
        return 0xff;
    case PATTERN_VERTICAL_STRIPES:
        return 0xaa;
    case PATTERN_CHECKERBOARD:
        return (line & 1U) ? 0x55 : 0xaa;
    default:
        return 0x00;
    }
}

static void build_wire_frame(lcd_pattern_t pattern)
{
    size_t pos = 0;

    /* SPI is LSB-first: 0x01 sends M0=1, M1=0, M2=0, then five dummy 0s. */
    s_wire_frame[pos++] = 0x01;

    for (unsigned int line = 1; line <= LCD_HEIGHT; ++line) {
        /* LSB-first sends line address AG0..AG7 in datasheet order. */
        s_wire_frame[pos++] = (uint8_t)line;
        memset(&s_wire_frame[pos], pattern_byte(pattern, line), LCD_BYTES_PER_LINE);
        pos += LCD_BYTES_PER_LINE;
        s_wire_frame[pos++] = 0x00; /* Per-line data-transfer dummy byte. */
    }

    s_wire_frame[pos++] = 0x00; /* Final trailing dummy byte. */
    configASSERT(pos == sizeof(s_wire_frame));
}

static esp_err_t lcd_write_full_frame(lcd_pattern_t pattern)
{
    esp_err_t err;
    spi_transaction_t transaction = {
        .length = sizeof(s_wire_frame) * 8,
        .tx_buffer = s_wire_frame,
    };

    xSemaphoreTake(s_spi_mutex, portMAX_DELAY);
    build_wire_frame(pattern);

    /* Sharp requires >=3 us SCS setup and >=1 us hold/low time. */
    gpio_set_level(LCD_GPIO_SCS, 1);
    esp_rom_delay_us(3);
    err = spi_device_polling_transmit(s_spi, &transaction);
    esp_rom_delay_us(1);
    gpio_set_level(LCD_GPIO_SCS, 0);
    esp_rom_delay_us(1);

    xSemaphoreGive(s_spi_mutex);
    return err;
}

static esp_err_t init_safe_gpio_low(void)
{
    const uint64_t mask = (1ULL << LCD_GPIO_SCLK) | (1ULL << LCD_GPIO_SI) |
                          (1ULL << LCD_GPIO_SCS) | (1ULL << LCD_GPIO_EXTCOMIN) |
                          (1ULL << LCD_GPIO_DISP);
    const gpio_config_t config = {
        .pin_bit_mask = mask,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_RETURN_ON_ERROR(gpio_config(&config), TAG, "GPIO safety configuration failed");
    gpio_set_level(LCD_GPIO_SCLK, 0);
    gpio_set_level(LCD_GPIO_SI, 0);
    gpio_set_level(LCD_GPIO_SCS, 0);
    gpio_set_level(LCD_GPIO_EXTCOMIN, 0);
    gpio_set_level(LCD_GPIO_DISP, 0);
    return ESP_OK;
}

static esp_err_t init_spi(void)
{
    const spi_bus_config_t bus = {
        .mosi_io_num = LCD_GPIO_SI,
        .miso_io_num = -1,
        .sclk_io_num = LCD_GPIO_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = sizeof(s_wire_frame),
    };
    const spi_device_interface_config_t device = {
        .clock_speed_hz = LCD_SPI_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = -1, /* SCS is manual to guarantee setup/hold timing. */
        .queue_size = 1,
        .flags = SPI_DEVICE_TXBIT_LSBFIRST,
    };

    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO), TAG,
                        "SPI bus initialization failed");
    ESP_RETURN_ON_ERROR(spi_bus_add_device(LCD_SPI_HOST, &device, &s_spi), TAG,
                        "SPI device initialization failed");
    return ESP_OK;
}

static esp_err_t start_extcomin(void)
{
    const ledc_timer_config_t timer = {
        .speed_mode = LCD_EXTCOMIN_MODE,
        /* ESP32-S3 LEDC is 14-bit. AUTO_CLK selects calibrated RC_FAST at 2 Hz. */
        .duty_resolution = LEDC_TIMER_14_BIT,
        .timer_num = LCD_EXTCOMIN_TIMER,
        .freq_hz = LCD_EXTCOMIN_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    const ledc_channel_config_t channel = {
        .gpio_num = LCD_GPIO_EXTCOMIN,
        .speed_mode = LCD_EXTCOMIN_MODE,
        .channel = LCD_EXTCOMIN_CHANNEL,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LCD_EXTCOMIN_TIMER,
        .duty = 1U << 13,
        .hpoint = 0,
        .flags = {
            .output_invert = 0,
        },
    };

    ESP_RETURN_ON_ERROR(ledc_timer_config(&timer), TAG, "EXTCOMIN timer setup failed");
    ESP_RETURN_ON_ERROR(ledc_channel_config(&channel), TAG, "EXTCOMIN output setup failed");

    const uint32_t actual_hz = ledc_get_freq(LCD_EXTCOMIN_MODE, LCD_EXTCOMIN_TIMER);
    ESP_RETURN_ON_FALSE(actual_hz == LCD_EXTCOMIN_HZ, ESP_FAIL, TAG,
                        "EXTCOMIN frequency mismatch: expected %d Hz, got %" PRIu32 " Hz",
                        LCD_EXTCOMIN_HZ, actual_hz);
    ESP_LOGI(TAG, "EXTCOMIN hardware output: %" PRIu32 " Hz, 50%% duty", actual_hz);
    return ESP_OK;
}

static void stop_extcomin_low(void)
{
    ledc_stop(LCD_EXTCOMIN_MODE, LCD_EXTCOMIN_CHANNEL, 0);
    gpio_set_direction(LCD_GPIO_EXTCOMIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_GPIO_EXTCOMIN, 0);
}

static void set_pattern(lcd_pattern_t pattern, bool auto_cycle)
{
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    s_pattern = pattern;
    s_refresh_enabled = true;
    s_auto_cycle = auto_cycle;
    s_pattern_started_us = esp_timer_get_time();
    xSemaphoreGive(s_state_mutex);
    printf("\n图案：%s；整帧刷新 4 Hz；EXTCOMIN 继续 2 Hz。\n", pattern_name(pattern));
}

static void print_status(void)
{
    lcd_pattern_t pattern;
    bool refresh;
    bool automatic;
    bool started;

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    pattern = s_pattern;
    refresh = s_refresh_enabled;
    automatic = s_auto_cycle;
    started = s_display_started;
    xSemaphoreGive(s_state_mutex);

    printf("\n状态：显示%s；图案=%s；像素刷新=%s；自动轮播=%s；EXTCOMIN=%s。\n",
           started ? "已启动" : "未启动/已关闭", pattern_name(pattern),
           refresh ? "4 Hz" : "停止（静态保持）", automatic ? "开" : "关",
           started ? "2 Hz 持续" : "低电平");
}

static void print_help(void)
{
    printf("\n命令：\n"
           "  cycle     自动：白/黑/竖条/棋盘格各 30 秒，然后静态保持\n"
           "  white     全白，4 Hz 重复刷新\n"
           "  black     全黑，4 Hz 重复刷新\n"
           "  stripes   竖条，4 Hz 重复刷新\n"
           "  checker   棋盘格，4 Hz 重复刷新\n"
           "  hold      停止像素写入；EXTCOMIN 仍保持 2 Hz\n"
           "  status    显示当前状态\n"
           "  shutdown  写白后安全停屏；随后手动拔掉显示 5V\n"
           "  help      再次显示本帮助\n\n");
}

static void refresh_task(void *unused)
{
    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(1000 / LCD_REFRESH_HZ);
    (void)unused;

    while (true) {
        lcd_pattern_t pattern;
        bool refresh;
        bool automatic;
        bool started;
        int64_t pattern_started;

        xSemaphoreTake(s_state_mutex, portMAX_DELAY);
        pattern = s_pattern;
        refresh = s_refresh_enabled;
        automatic = s_auto_cycle;
        started = s_display_started;
        pattern_started = s_pattern_started_us;
        xSemaphoreGive(s_state_mutex);

        if (started && automatic &&
            esp_timer_get_time() - pattern_started >= LCD_PATTERN_SECONDS * 1000000LL) {
            if (pattern + 1 < PATTERN_COUNT) {
                set_pattern((lcd_pattern_t)(pattern + 1), true);
            } else {
                xSemaphoreTake(s_state_mutex, portMAX_DELAY);
                s_refresh_enabled = false;
                s_auto_cycle = false;
                xSemaphoreGive(s_state_mutex);
                printf("\n自动测试完成：进入棋盘格静态保持。像素写入已停止，EXTCOMIN 仍为 2 Hz。\n"
                       "请至少观察 30 秒；输入 help 可查看后续命令。\n");
            }
        } else if (started && refresh) {
            esp_err_t err = lcd_write_full_frame(pattern);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "frame write failed: %s", esp_err_to_name(err));
            }
        }

        vTaskDelayUntil(&last_wake, period);
    }
}

static void trim_line(char *line)
{
    size_t length = strlen(line);
    while (length > 0 && isspace((unsigned char)line[length - 1])) {
        line[--length] = '\0';
    }
    char *start = line;
    while (*start != '\0' && isspace((unsigned char)*start)) {
        ++start;
    }
    if (start != line) {
        memmove(line, start, strlen(start) + 1);
    }
}

static void wait_for_confirmation(const char *expected)
{
    char line[64];
    while (true) {
        printf("> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        trim_line(line);
        if (strcasecmp(line, expected) == 0) {
            return;
        }
        printf("未执行。请输入 %s（不要提前接显示 5V）。\n", expected);
    }
}

static void safe_shutdown(void)
{
    bool started;
    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    started = s_display_started;
    s_refresh_enabled = false;
    s_auto_cycle = false;
    s_display_started = false;
    xSemaphoreGive(s_state_mutex);

    if (!started) {
        printf("显示已经处于关闭状态。\n");
        return;
    }

    printf("正在执行停屏：先写全白，并在 EXTCOMIN 运行时等待 1 秒……\n");
    ESP_ERROR_CHECK(lcd_write_full_frame(PATTERN_WHITE));
    vTaskDelay(pdMS_TO_TICKS(1000));
    gpio_set_level(LCD_GPIO_DISP, 0);
    esp_rom_delay_us(30);
    stop_extcomin_low();
    gpio_set_level(LCD_GPIO_SCS, 0);
    gpio_set_level(LCD_GPIO_SI, 0);
    gpio_set_level(LCD_GPIO_SCLK, 0);
    printf("显示信号已拉低。现在可以手动拔掉显示 5V；如需重测，请先拔 5V 再复位。\n");
}

static void command_loop(void)
{
    char line[64];
    print_help();
    while (true) {
        printf("l1-display> ");
        fflush(stdout);
        if (fgets(line, sizeof(line), stdin) == NULL) {
            clearerr(stdin);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        trim_line(line);

        if (strcasecmp(line, "cycle") == 0) {
            set_pattern(PATTERN_WHITE, true);
        } else if (strcasecmp(line, "white") == 0) {
            set_pattern(PATTERN_WHITE, false);
        } else if (strcasecmp(line, "black") == 0) {
            set_pattern(PATTERN_BLACK, false);
        } else if (strcasecmp(line, "stripes") == 0) {
            set_pattern(PATTERN_VERTICAL_STRIPES, false);
        } else if (strcasecmp(line, "checker") == 0) {
            set_pattern(PATTERN_CHECKERBOARD, false);
        } else if (strcasecmp(line, "hold") == 0) {
            xSemaphoreTake(s_state_mutex, portMAX_DELAY);
            s_refresh_enabled = false;
            s_auto_cycle = false;
            xSemaphoreGive(s_state_mutex);
            printf("像素写入已停止；屏幕内容静态保持，EXTCOMIN 仍持续 2 Hz。\n");
        } else if (strcasecmp(line, "status") == 0) {
            print_status();
        } else if (strcasecmp(line, "shutdown") == 0) {
            safe_shutdown();
        } else if (strcasecmp(line, "help") == 0 || line[0] == '\0') {
            print_help();
        } else {
            printf("未知命令：%s\n", line);
            print_help();
        }
    }
}

void app_main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    s_spi_mutex = xSemaphoreCreateMutex();
    s_state_mutex = xSemaphoreCreateMutex();
    configASSERT(s_spi_mutex != NULL);
    configASSERT(s_state_mutex != NULL);

    ESP_ERROR_CHECK(init_safe_gpio_low());

    printf("\n============================================================\n"
           "Cadenza L1 / Sharp LS027B7DH01 面包板验证固件\n"
           "GPIO 已安全拉低：SCLK39 SI48 SCS47 EXTCOMIN14 DISP12\n"
           "此固件不能控制显示 5V；5V 必须由你最后手动插入。\n"
           "============================================================\n\n"
           "确认：USB 已接通，但显示 5V 跳线仍未插；FPC/Pin 1 已用万用表核对；\n"
           "三颗电容已接好，且 5V 与 GND 不短路。满足后输入 ARM。\n");
    wait_for_confirmation("ARM");

    printf("\nGPIO 仍全部为低。现在才插入显示 5V 跳线。\n"
           "用万用表测屏幕 pin 6→9 和 pin 7→9，必须都在 4.8–5.5 V。\n"
           "电压正确后输入 GO；不正确就立刻拔掉 5V，不要输入 GO。\n");
    wait_for_confirmation("GO");

    ESP_ERROR_CHECK(init_spi());
    printf("\n先在 DISP=0、EXTCOMIN=0 时写入整屏白色作为像素存储初始化……\n");
    ESP_ERROR_CHECK(lcd_write_full_frame(PATTERN_WHITE));

    gpio_set_level(LCD_GPIO_DISP, 1);
    esp_rom_delay_us(30);
    ESP_ERROR_CHECK(start_extcomin());
    esp_rom_delay_us(30);

    xSemaphoreTake(s_state_mutex, portMAX_DELAY);
    s_display_started = true;
    s_pattern = PATTERN_WHITE;
    s_refresh_enabled = true;
    s_auto_cycle = true;
    s_pattern_started_us = esp_timer_get_time();
    xSemaphoreGive(s_state_mutex);

    BaseType_t task_result = xTaskCreate(refresh_task, "ls027_refresh", 4096, NULL, 5, NULL);
    configASSERT(task_result == pdPASS);

    printf("显示已启动：SPI mode 0、LSB-first、1 MHz；EXTCOMIN 2 Hz。\n"
           "现在自动运行白/黑/竖条/棋盘格，每种 30 秒、整帧刷新 4 Hz，最后静态保持。\n");
    command_loop();
}
