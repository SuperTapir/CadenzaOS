#pragma once

#include <cstdint>

using BaseType_t = int;
using UBaseType_t = unsigned int;
using TickType_t = std::uint32_t;
using TaskHandle_t = void*;
using TaskFunction_t = void (*)(void*);

constexpr BaseType_t pdPASS = 1;

#define pdMS_TO_TICKS(milliseconds) static_cast<TickType_t>(milliseconds)

BaseType_t xTaskCreatePinnedToCore(TaskFunction_t task, const char* name,
                                  std::uint32_t stackBytes, void* context,
                                  UBaseType_t priority, TaskHandle_t* handle,
                                  BaseType_t core);
void vTaskDelete(TaskHandle_t task);
void delay(unsigned long milliseconds);
