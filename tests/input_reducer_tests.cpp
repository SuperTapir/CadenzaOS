#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <limits>

#include "cadenza/core/input.h"
#include "cadenza/core/input_adapter.h"

namespace {
constexpr cadenza::InputConfig kConfig{10, 100};

cadenza::RawInputEvent button(bool down, std::uint64_t at) {
  return {down ? cadenza::RawInputType::ButtonDown
               : cadenza::RawInputType::ButtonUp,
          at, 0};
}

cadenza::RawInputEvent turn(std::int32_t delta, std::uint64_t at = 0) {
  return {cadenza::RawInputType::Turn, at, delta};
}
}  // namespace

TEST_CASE("button debounce produces one press and repeated samples do not reset it") {
  cadenza::InputReducer input{kConfig};
  input.push(button(true, 0));
  input.push(button(true, 5));
  input.advanceTo(9);
  CHECK_FALSE(input.takeFrame().pressed);

  input.advanceTo(10);
  const auto pressed = input.takeFrame();
  CHECK(pressed.pressed);
  CHECK(pressed.heldMs == 0);

  input.advanceTo(25);
  const auto held = input.takeFrame();
  CHECK_FALSE(held.pressed);
  CHECK(held.heldMs == 15);
}

TEST_CASE("release immediately before the inclusive long-press boundary clicks") {
  cadenza::InputReducer input{kConfig};
  input.push(button(true, 0));
  input.advanceTo(10);
  input.takeFrame();
  input.push(button(false, 99));
  input.advanceTo(109);

  const auto frame = input.takeFrame();
  CHECK(frame.released);
  CHECK(frame.clicked);
  CHECK_FALSE(frame.longPressed);
  CHECK(frame.heldMs == 0);
}

TEST_CASE("release at or after the inclusive long-press boundary does not click") {
  for (const std::uint64_t releaseSampleAt : {100ULL, 101ULL}) {
    CAPTURE(releaseSampleAt);
    cadenza::InputReducer input{kConfig};
    input.push(button(true, 0));
    input.advanceTo(10);
    input.takeFrame();
    input.push(button(false, releaseSampleAt));
    input.advanceTo(releaseSampleAt + kConfig.debounceMs);

    const auto frame = input.takeFrame();
    CHECK(frame.longPressed);
    CHECK(frame.released);
    CHECK_FALSE(frame.clicked);
  }
}

TEST_CASE("long press is emitted once while held") {
  cadenza::InputReducer input{kConfig};
  input.push(button(true, 0));
  input.advanceTo(110);
  auto frame = input.takeFrame();
  CHECK(frame.pressed);
  CHECK(frame.longPressed);
  CHECK(frame.heldMs == 100);

  input.advanceTo(300);
  frame = input.takeFrame();
  CHECK_FALSE(frame.longPressed);
  CHECK(frame.heldMs == 290);
}

TEST_CASE("turn deltas preserve direction and saturate without wrapping") {
  cadenza::InputReducer input{kConfig};
  input.push(turn(12));
  input.push(turn(-2));
  CHECK(input.takeFrame().turn == 10);

  input.push(turn(std::numeric_limits<std::int32_t>::max()));
  CHECK(input.takeFrame().turn == std::numeric_limits<std::int16_t>::max());
  input.push(turn(std::numeric_limits<std::int32_t>::min()));
  CHECK(input.takeFrame().turn == std::numeric_limits<std::int16_t>::min());
  CHECK(input.takeFrame().turn == 0);
}

TEST_CASE("frame reset clears transients but preserves current held duration") {
  cadenza::InputReducer input{kConfig};
  input.push(button(true, 0));
  input.advanceTo(50);
  input.push(turn(1, 50));

  const auto first = input.takeFrame();
  CHECK(first.pressed);
  CHECK(first.turn == 1);
  CHECK(first.heldMs == 40);

  const auto second = input.takeFrame();
  CHECK_FALSE(second.pressed);
  CHECK_FALSE(second.released);
  CHECK_FALSE(second.clicked);
  CHECK_FALSE(second.longPressed);
  CHECK(second.turn == 0);
  CHECK(second.heldMs == 40);
}

TEST_CASE("raw input and monotonic time enter through non-owning adapters") {
  class FakeClock final : public cadenza::MonotonicClock {
   public:
    cadenza::MonotonicMillis nowMs() const noexcept override { return now; }
    cadenza::MonotonicMillis now = 10;
  } clock;

  class FakeSource final : public cadenza::RawInputSource {
   public:
    bool poll(cadenza::RawInputEvent& event) noexcept override {
      if (position == 2) {
        return false;
      }
      const cadenza::RawInputEvent events[] = {turn(3, 0), button(true, 0)};
      event = events[position++];
      return true;
    }
    int position = 0;
  } source;

  cadenza::InputReducer input{kConfig};
  cadenza::pumpInput(source, clock, input);
  const auto frame = input.takeFrame();
  CHECK(frame.turn == 3);
  CHECK(frame.pressed);
  CHECK(source.position == 2);

  clock.now = 110;
  cadenza::pumpInput(source, clock, input);
  CHECK(input.takeFrame().longPressed);
}
