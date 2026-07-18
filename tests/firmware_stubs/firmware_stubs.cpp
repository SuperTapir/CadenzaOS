#include "firmware_stubs.h"

#include <algorithm>

namespace firmware_stubs {
namespace {
State value;
}

State& state() { return value; }
void reset() { value = {}; }
}  // namespace firmware_stubs

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t task, const char*,
                                  std::uint32_t, void* context, UBaseType_t,
                                  TaskHandle_t* handle, BaseType_t) {
  auto& state = firmware_stubs::state();
  ++state.taskCalls;
  state.capturedTask = task;
  state.capturedContext = context;
  if (state.taskResult == pdPASS && handle) {
    *handle = reinterpret_cast<TaskHandle_t>(0x1);
  }
  return state.taskResult;
}

void vTaskDelete(TaskHandle_t) {}
void delay(unsigned long) {}

esp_err_t i2s_driver_install(i2s_port_t, const i2s_config_t* config, int,
                             void*) {
  auto& state = firmware_stubs::state();
  ++state.installCalls;
  if (config) state.config = *config;
  return state.installResult;
}

esp_err_t i2s_set_pin(i2s_port_t, const i2s_pin_config_t* pins) {
  auto& state = firmware_stubs::state();
  ++state.pinCalls;
  if (pins) state.pins = *pins;
  return state.pinResult;
}

esp_err_t i2s_write(i2s_port_t, const void*, std::size_t bytes,
                    std::size_t* written, TickType_t) {
  auto& state = firmware_stubs::state();
  ++state.writeCalls;
  const esp_err_t result = state.writeCalls == 1 ? state.writeResult
                                                  : state.writeResultAfterFirst;
  if (written) {
    *written = state.writeBytes == static_cast<std::size_t>(-1)
                   ? bytes
                   : std::min(bytes, state.writeBytes);
  }
  return result;
}

esp_err_t i2s_zero_dma_buffer(i2s_port_t) {
  ++firmware_stubs::state().zeroCalls;
  return ESP_OK;
}

esp_err_t i2s_driver_uninstall(i2s_port_t) {
  ++firmware_stubs::state().uninstallCalls;
  return ESP_OK;
}
