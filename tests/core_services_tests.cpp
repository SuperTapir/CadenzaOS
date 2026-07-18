#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstddef>

#include "cadenza/core/diagnostics.h"
#include "cadenza/core/simulation_clock.h"

namespace {
class RecordingSink final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    if (size_ < events_.size()) {
      events_[size_++] = event;
    }
  }

  std::size_t size() const noexcept { return size_; }
  const cadenza::DiagnosticEvent& operator[](std::size_t index) const {
    return events_[index];
  }

 private:
  std::array<cadenza::DiagnosticEvent, 4> events_{};
  std::size_t size_ = 0;
};
}  // namespace

TEST_CASE("diagnostics are optional and preserve structured fields") {
  cadenza::NoOpDiagnosticSink no_op;
  const cadenza::DiagnosticEvent event{
      cadenza::DiagnosticCategory::Runtime,
      cadenza::DiagnosticCode::InvalidOperation,
      "open rejected",
      7,
  };

  CHECK_NOTHROW(no_op.emit(event));

  RecordingSink recording;
  recording.emit(event);
  REQUIRE(recording.size() == 1);
  CHECK(recording[0].category == cadenza::DiagnosticCategory::Runtime);
  CHECK(recording[0].code == cadenza::DiagnosticCode::InvalidOperation);
  CHECK(recording[0].value == 7);
}

TEST_CASE("simulation time advances only from injected non-negative delta") {
  cadenza::SimulationClock clock;

  const auto first = clock.advance(0.016F);
  CHECK(first.deltaSeconds == doctest::Approx(0.016F));
  CHECK(first.elapsedSeconds == doctest::Approx(0.016F));
  CHECK(first.frameIndex == 1);

  const auto paused = clock.advance(0.0F);
  CHECK(paused.deltaSeconds == 0.0F);
  CHECK(paused.elapsedSeconds == doctest::Approx(0.016F));
  CHECK(paused.frameIndex == 1);

  const auto invalid = clock.advance(-1.0F);
  CHECK(invalid.deltaSeconds == 0.0F);
  CHECK(invalid.elapsedSeconds == doctest::Approx(0.016F));
  CHECK(invalid.frameIndex == 1);
}

TEST_CASE("simulation clock reset is deterministic") {
  cadenza::SimulationClock clock;
  clock.advance(0.5F);
  clock.reset(2.0F);

  CHECK(clock.elapsedSeconds() == doctest::Approx(2.0F));
  CHECK(clock.frameIndex() == 0);
  const auto frame = clock.advance(0.25F);
  CHECK(frame.elapsedSeconds == doctest::Approx(2.25F));
  CHECK(frame.frameIndex == 1);
}
