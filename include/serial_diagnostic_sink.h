#pragma once

#include <Arduino.h>

#include "cadenza/core/diagnostics.h"

class SerialDiagnosticSink final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    Serial.printf("[core:%u/%u] %s (%ld)\n",
                  static_cast<unsigned>(event.category),
                  static_cast<unsigned>(event.code),
                  event.message ? event.message : "", static_cast<long>(event.value));
  }
};
