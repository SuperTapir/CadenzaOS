#pragma once

#include <cstdint>

namespace cadenza {

enum class DiagnosticCategory : std::uint8_t {
  Runtime,
  Input,
  Graphics,
  Animation,
  Capacity,
};

enum class DiagnosticCode : std::uint8_t {
  AppTransition,
  SelectionChanged,
  InvalidOperation,
  InvalidGeometry,
  FullyClipped,
  CapacityExceeded,
};

struct DiagnosticEvent {
  DiagnosticCategory category = DiagnosticCategory::Runtime;
  DiagnosticCode code = DiagnosticCode::InvalidOperation;
  const char* message = nullptr;
  std::int32_t value = 0;
};

class DiagnosticSink {
 public:
  virtual ~DiagnosticSink() = default;
  virtual void emit(const DiagnosticEvent& event) noexcept = 0;
};

class NoOpDiagnosticSink final : public DiagnosticSink {
 public:
  void emit(const DiagnosticEvent&) noexcept override {}
};

}  // namespace cadenza
