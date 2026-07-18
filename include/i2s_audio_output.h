#pragma once

#include <Arduino.h>
#include <driver/i2s.h>

#include <atomic>

#include "cadenza/core/app_runtime.h"
#include "cadenza/core/diagnostics.h"

class I2sAudioOutput {
 public:
  bool begin(cadenza::AppRuntime& runtime,
             cadenza::DiagnosticSink* diagnostics = nullptr) noexcept;
  void stop() noexcept;
  bool running() const noexcept {
    return running_.load(std::memory_order_acquire);
  }

 private:
  static void taskEntry(void* context) noexcept;
  void run() noexcept;
  void emit(cadenza::DiagnosticCode code, const char* message,
            std::int32_t value = 0) const noexcept;

  cadenza::AppRuntime* runtime_ = nullptr;
  cadenza::DiagnosticSink* diagnostics_ = nullptr;
  std::atomic<bool> running_{false};
  std::atomic<TaskHandle_t> task_{nullptr};
  bool driverInstalled_ = false;
};
