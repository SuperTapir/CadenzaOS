#pragma once

#include <Arduino.h>

#include "cadenza/core/diagnostics.h"

class SerialDiagnosticSink final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    // Geometry clip/invalid noise is expected during Gallery demos and shake.
    // Printing every event over USB-CDC blocks loop() long enough for the
    // task watchdog to reset when a heavy Transition page (e.g. DIP) is up.
    if (event.category == cadenza::DiagnosticCategory::Graphics &&
        (event.code == cadenza::DiagnosticCode::ClippedGeometry ||
         event.code == cadenza::DiagnosticCode::FullyClipped ||
         event.code == cadenza::DiagnosticCode::InvalidGeometry)) {
      return;
    }
    Serial.printf("[core:%u/%u] %s (%ld)\n",
                  static_cast<unsigned>(event.category),
                  static_cast<unsigned>(event.code),
                  event.message ? event.message : "",
                  static_cast<long>(event.value));
  }
};
