#include "t_embed_display.h"

#include <algorithm>
#include <cstddef>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_io_spi.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

namespace cadenza::idf {
namespace {

constexpr spi_host_device_t kSpiHost = SPI2_HOST;
constexpr int kPowerPin = 46;
constexpr int kBacklightPin = 15;
constexpr int kMosiPin = 11;
constexpr int kClockPin = 12;
constexpr int kChipSelectPin = 10;
constexpr int kDataCommandPin = 13;
constexpr int kResetPin = 9;

bool colorTransferDone(esp_lcd_panel_io_handle_t,
                       esp_lcd_panel_io_event_data_t*, void* context) {
  BaseType_t higherPriorityTaskWoken = pdFALSE;
  xSemaphoreGiveFromISR(static_cast<SemaphoreHandle_t>(context),
                        &higherPriorityTaskWoken);
  return higherPriorityTaskWoken == pdTRUE;
}

struct PanelCommand {
  std::uint8_t command;
  std::array<std::uint8_t, 14> data;
  std::uint8_t length;
};

constexpr PanelCommand kPanelCommands[] = {
    {0xB2, {0x0B, 0x0B, 0x00, 0x33, 0x33}, 5},
    {0xB7, {0x75}, 1},
    {0xBB, {0x28}, 1},
    {0xC0, {0x2C}, 1},
    {0xC2, {0x01}, 1},
    {0xC3, {0x1F}, 1},
    {0xC6, {0x13}, 1},
    {0xD0, {0xA4, 0xA1}, 2},
    {0xD6, {0xA1}, 1},
    {0xE0,
     {0xF0, 0x05, 0x0A, 0x06, 0x06, 0x03, 0x2B, 0x32, 0x43, 0x36, 0x11,
      0x10, 0x2B, 0x32},
     14},
    {0xE1,
     {0xF0, 0x08, 0x0C, 0x0B, 0x09, 0x24, 0x2B, 0x22, 0x43, 0x38, 0x15,
      0x16, 0x2F, 0x37},
     14},
};

}  // namespace

TEmbedDisplay::~TEmbedDisplay() { release(); }

esp_err_t TEmbedDisplay::start() noexcept {
  transferDone_ = xSemaphoreCreateBinary();
  if (transferDone_ == nullptr) return ESP_ERR_NO_MEM;
  xSemaphoreGive(static_cast<SemaphoreHandle_t>(transferDone_));

  gpio_config_t outputs{};
  outputs.pin_bit_mask = (1ULL << kPowerPin) | (1ULL << kBacklightPin);
  outputs.mode = GPIO_MODE_OUTPUT;
  esp_err_t result = gpio_config(&outputs);
  if (result != ESP_OK) {
    release();
    return result;
  }
  gpio_set_level(static_cast<gpio_num_t>(kBacklightPin), 0);
  gpio_set_level(static_cast<gpio_num_t>(kPowerPin), 1);
  vTaskDelay(pdMS_TO_TICKS(20));

  spi_bus_config_t bus{};
  bus.mosi_io_num = kMosiPin;
  bus.miso_io_num = -1;
  bus.sclk_io_num = kClockPin;
  bus.quadwp_io_num = -1;
  bus.quadhd_io_num = -1;
  bus.max_transfer_sz = transfer_.size() * sizeof(transfer_[0]);
  result = spi_bus_initialize(kSpiHost, &bus, SPI_DMA_CH_AUTO);
  if (result != ESP_OK) {
    release();
    return result;
  }
  busInitialized_ = true;

  esp_lcd_panel_io_spi_config_t ioConfig{};
  ioConfig.cs_gpio_num = kChipSelectPin;
  ioConfig.dc_gpio_num = kDataCommandPin;
  ioConfig.spi_mode = 0;
  // Match PlatformIO TFT_eSPI SPI_FREQUENCY=80MHz on short T-Embed traces.
  ioConfig.pclk_hz = 80'000'000;
  ioConfig.trans_queue_depth = 2;
  ioConfig.on_color_trans_done = colorTransferDone;
  ioConfig.user_ctx = transferDone_;
  ioConfig.lcd_cmd_bits = 8;
  ioConfig.lcd_param_bits = 8;
  auto* io = reinterpret_cast<esp_lcd_panel_io_handle_t*>(&panelIo_);
  result = esp_lcd_new_panel_io_spi(
      static_cast<esp_lcd_spi_bus_handle_t>(kSpiHost), &ioConfig, io);
  if (result != ESP_OK) {
    release();
    return result;
  }

  esp_lcd_panel_dev_config_t panelConfig{};
  panelConfig.reset_gpio_num = kResetPin;
  panelConfig.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB;
  panelConfig.bits_per_pixel = 16;
  auto* panel = reinterpret_cast<esp_lcd_panel_handle_t*>(&panel_);
  result = esp_lcd_new_panel_st7789(
      reinterpret_cast<esp_lcd_panel_io_handle_t>(panelIo_), &panelConfig,
      panel);
  if (result != ESP_OK) {
    release();
    return result;
  }

  auto handle = reinterpret_cast<esp_lcd_panel_handle_t>(panel_);
  // Landscape + MY|MV — 180° from the previous MX|MV mapping so the T-Embed
  // UI matches the held orientation (PlatformIO setRotation(3) feel on this
  // panel via esp_lcd).
  if ((result = esp_lcd_panel_reset(handle)) != ESP_OK ||
      (result = esp_lcd_panel_init(handle)) != ESP_OK ||
      (result = esp_lcd_panel_invert_color(handle, true)) != ESP_OK ||
      (result = esp_lcd_panel_swap_xy(handle, true)) != ESP_OK ||
      (result = esp_lcd_panel_mirror(handle, false, true)) != ESP_OK ||
      (result = esp_lcd_panel_set_gap(handle, 0, 35)) != ESP_OK) {
    release();
    return result;
  }
  for (const auto& command : kPanelCommands) {
    result = esp_lcd_panel_io_tx_param(
        reinterpret_cast<esp_lcd_panel_io_handle_t>(panelIo_),
        command.command, command.data.data(), command.length);
    if (result != ESP_OK) {
      release();
      return result;
    }
  }
  result = esp_lcd_panel_disp_on_off(handle, true);
  if (result == ESP_OK) {
    gpio_set_level(static_cast<gpio_num_t>(kBacklightPin), 1);
  } else {
    release();
  }
  return result;
}

bool TEmbedDisplay::present(const MonoFramebuffer& framebuffer) noexcept {
  if (panel_ == nullptr || !framebuffer.valid() || framebuffer.width() != 320 ||
      framebuffer.height() != 170) {
    return false;
  }
  // Bulk 1-bit → RGB565 conversion (same unroll as PlatformIO TftPresenter).
  const std::uint8_t* data = framebuffer.data();
  const std::size_t stride = framebuffer.stride();
  for (std::int16_t y = 0; y < framebuffer.height();
       y += static_cast<std::int16_t>(kRowsPerTransfer)) {
    if (xSemaphoreTake(static_cast<SemaphoreHandle_t>(transferDone_),
                       pdMS_TO_TICKS(50)) != pdTRUE) {
      return false;
    }
    const auto rows = static_cast<std::size_t>(std::min<std::int16_t>(
        static_cast<std::int16_t>(kRowsPerTransfer), framebuffer.height() - y));
    for (std::size_t row = 0; row < rows; ++row) {
      const std::uint8_t* source =
          data + static_cast<std::size_t>(y + static_cast<std::int16_t>(row)) *
                     stride;
      std::uint16_t* dest = transfer_.data() + row * kWidth;
      std::size_t x = 0;
      while (x + 8 <= kWidth) {
        const std::uint8_t cell = source[x >> 3];
        dest[x + 0] = (cell & 0x80U) != 0 ? 0x0000 : 0xFFFF;
        dest[x + 1] = (cell & 0x40U) != 0 ? 0x0000 : 0xFFFF;
        dest[x + 2] = (cell & 0x20U) != 0 ? 0x0000 : 0xFFFF;
        dest[x + 3] = (cell & 0x10U) != 0 ? 0x0000 : 0xFFFF;
        dest[x + 4] = (cell & 0x08U) != 0 ? 0x0000 : 0xFFFF;
        dest[x + 5] = (cell & 0x04U) != 0 ? 0x0000 : 0xFFFF;
        dest[x + 6] = (cell & 0x02U) != 0 ? 0x0000 : 0xFFFF;
        dest[x + 7] = (cell & 0x01U) != 0 ? 0x0000 : 0xFFFF;
        x += 8;
      }
    }
    if (esp_lcd_panel_draw_bitmap(
            reinterpret_cast<esp_lcd_panel_handle_t>(panel_), 0, y, kWidth,
            y + static_cast<std::int16_t>(rows), transfer_.data()) != ESP_OK) {
      xSemaphoreGive(static_cast<SemaphoreHandle_t>(transferDone_));
      return false;
    }
  }
  // Wait for the final DMA chunk so the next frame never starts converting
  // into `transfer_` while SPI is still reading it.
  if (xSemaphoreTake(static_cast<SemaphoreHandle_t>(transferDone_),
                     pdMS_TO_TICKS(50)) != pdTRUE) {
    return false;
  }
  xSemaphoreGive(static_cast<SemaphoreHandle_t>(transferDone_));
  return true;
}

void TEmbedDisplay::release() noexcept {
  gpio_set_level(static_cast<gpio_num_t>(kBacklightPin), 0);
  if (transferDone_ != nullptr) {
    xSemaphoreTake(static_cast<SemaphoreHandle_t>(transferDone_),
                   pdMS_TO_TICKS(50));
  }
  if (panel_ != nullptr) {
    esp_lcd_panel_del(reinterpret_cast<esp_lcd_panel_handle_t>(panel_));
    panel_ = nullptr;
  }
  if (panelIo_ != nullptr) {
    esp_lcd_panel_io_del(
        reinterpret_cast<esp_lcd_panel_io_handle_t>(panelIo_));
    panelIo_ = nullptr;
  }
  if (busInitialized_) {
    spi_bus_free(kSpiHost);
    busInitialized_ = false;
  }
  if (transferDone_ != nullptr) {
    vSemaphoreDelete(static_cast<SemaphoreHandle_t>(transferDone_));
    transferDone_ = nullptr;
  }
}

}  // namespace cadenza::idf
