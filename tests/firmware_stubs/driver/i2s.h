#pragma once

#include <cstddef>
#include <cstdint>

#include "Arduino.h"

using esp_err_t = std::int32_t;
constexpr esp_err_t ESP_OK = 0;
constexpr int ESP_INTR_FLAG_LEVEL1 = 1;

enum i2s_port_t { I2S_NUM_0 = 0 };
enum i2s_mode_t { I2S_MODE_MASTER = 1, I2S_MODE_TX = 2 };
enum i2s_bits_per_sample_t { I2S_BITS_PER_SAMPLE_16BIT = 16 };
enum i2s_channel_fmt_t { I2S_CHANNEL_FMT_RIGHT_LEFT = 0 };
enum i2s_comm_format_t { I2S_COMM_FORMAT_STAND_I2S = 0 };

constexpr int I2S_PIN_NO_CHANGE = -1;

struct i2s_config_t {
  i2s_mode_t mode{};
  std::uint32_t sample_rate = 0;
  i2s_bits_per_sample_t bits_per_sample{};
  i2s_channel_fmt_t channel_format{};
  i2s_comm_format_t communication_format{};
  int intr_alloc_flags = 0;
  int dma_buf_count = 0;
  int dma_buf_len = 0;
  bool use_apll = false;
  bool tx_desc_auto_clear = false;
  int fixed_mclk = 0;
};

struct i2s_pin_config_t {
  int mck_io_num = I2S_PIN_NO_CHANGE;
  int bck_io_num = I2S_PIN_NO_CHANGE;
  int ws_io_num = I2S_PIN_NO_CHANGE;
  int data_out_num = I2S_PIN_NO_CHANGE;
  int data_in_num = I2S_PIN_NO_CHANGE;
};

esp_err_t i2s_driver_install(i2s_port_t port, const i2s_config_t* config,
                             int queueSize, void* queue);
esp_err_t i2s_set_pin(i2s_port_t port, const i2s_pin_config_t* pins);
esp_err_t i2s_write(i2s_port_t port, const void* source, std::size_t bytes,
                    std::size_t* written, TickType_t timeout);
esp_err_t i2s_zero_dma_buffer(i2s_port_t port);
esp_err_t i2s_driver_uninstall(i2s_port_t port);
