#pragma once

#include <cstddef>

#include "Arduino.h"
#include "driver/i2s.h"

namespace firmware_stubs {

struct State {
  esp_err_t installResult = ESP_OK;
  esp_err_t pinResult = ESP_OK;
  BaseType_t taskResult = pdPASS;
  esp_err_t writeResult = ESP_OK;
  std::size_t writeBytes = static_cast<std::size_t>(-1);
  esp_err_t writeResultAfterFirst = ESP_OK;
  int installCalls = 0;
  int pinCalls = 0;
  int taskCalls = 0;
  int writeCalls = 0;
  int uninstallCalls = 0;
  int zeroCalls = 0;
  TaskFunction_t capturedTask = nullptr;
  void* capturedContext = nullptr;
  i2s_config_t config{};
  i2s_pin_config_t pins{};
};

State& state();
void reset();

}  // namespace firmware_stubs
