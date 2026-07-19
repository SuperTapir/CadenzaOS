#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <new>

#include "cadenza/core/timer_types.h"
#include "cadenza/system/timer_service.h"

namespace {
std::size_t gAllocationCount = 0;
}

void* operator new(std::size_t size) {
  if (void* memory = std::malloc(size)) {
    ++gAllocationCount;
    return memory;
  }
  throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
  if (void* memory = std::malloc(size)) {
    ++gAllocationCount;
    return memory;
  }
  throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept {
  std::free(memory);
}

namespace {
constexpr cadenza::AppId kOwner{0x0101};
constexpr cadenza::AppId kOther{0x0102};
constexpr std::uint64_t minutes(std::uint64_t value) noexcept {
  return value * cadenza::kTimerMinuteMs;
}
}  // namespace

TEST_CASE("Timer starts Ready at ten minutes and accepts inclusive limits") {
  cadenza::system::TimerService timer;
  CHECK(timer.snapshot().state == cadenza::TimerState::Ready);
  CHECK(timer.snapshot().configuredDurationMs == minutes(10));
  CHECK(timer.snapshot().remainingMs == minutes(10));

  timer.advanceTo(1000);
  REQUIRE(timer.start(kOwner, minutes(1)) ==
          cadenza::system::TimerRequestResult::Accepted);
  CHECK(timer.snapshot().state == cadenza::TimerState::Running);
  CHECK(timer.snapshot().remainingMs == minutes(1));

  cadenza::system::TimerService maximum;
  maximum.advanceTo(25);
  REQUIRE(maximum.start(kOwner, minutes(99)) ==
          cadenza::system::TimerRequestResult::Accepted);
  CHECK(maximum.snapshot().remainingMs == minutes(99));
}

TEST_CASE("Timer deadline is inclusive and expiration generation advances once") {
  cadenza::system::TimerService timer;
  timer.advanceTo(1000);
  REQUIRE(timer.start(kOwner, minutes(1)) ==
          cadenza::system::TimerRequestResult::Accepted);

  CHECK_FALSE(timer.advanceTo(60999));
  CHECK(timer.snapshot().remainingMs == 1);
  CHECK(timer.snapshot().expirationGeneration == 0);
  CHECK(timer.advanceTo(61000));
  CHECK(timer.snapshot().state == cadenza::TimerState::Expired);
  CHECK(timer.snapshot().remainingMs == 0);
  CHECK(timer.snapshot().expirationGeneration == 1);
  CHECK_FALSE(timer.advanceTo(61001));
  CHECK_FALSE(timer.advanceTo(minutes(120)));
  CHECK(timer.snapshot().expirationGeneration == 1);
}

TEST_CASE("Pause freezes remaining time and resume creates a new deadline") {
  cadenza::system::TimerService timer;
  timer.advanceTo(1000);
  REQUIRE(timer.start(kOwner, minutes(10)) ==
          cadenza::system::TimerRequestResult::Accepted);
  timer.advanceTo(43000);
  REQUIRE(timer.pause(kOwner) ==
          cadenza::system::TimerRequestResult::Accepted);
  CHECK(timer.snapshot().state == cadenza::TimerState::Paused);
  CHECK(timer.snapshot().remainingMs == minutes(10) - 42000);

  timer.advanceTo(minutes(30));
  CHECK(timer.snapshot().remainingMs == minutes(10) - 42000);
  REQUIRE(timer.resume(kOwner) ==
          cadenza::system::TimerRequestResult::Accepted);
  CHECK_FALSE(timer.advanceTo(minutes(30) + minutes(10) - 42001));
  CHECK(timer.advanceTo(minutes(30) + minutes(10) - 42000));
}

TEST_CASE("Paused adjustment preserves seconds and clamps to timer limits") {
  cadenza::system::TimerService timer;
  timer.advanceTo(0);
  REQUIRE(timer.start(kOwner, minutes(10)) ==
          cadenza::system::TimerRequestResult::Accepted);
  timer.advanceTo(minutes(2) + 42000);
  REQUIRE(timer.pause(kOwner) ==
          cadenza::system::TimerRequestResult::Accepted);
  REQUIRE(timer.setRemaining(kOwner, minutes(9) + 18000) ==
          cadenza::system::TimerRequestResult::Accepted);
  CHECK(timer.snapshot().remainingMs == minutes(9) + 18000);
  CHECK(timer.snapshot().configuredDurationMs == minutes(9) + 18000);
}

TEST_CASE("Monotonic-time regression never increases remaining time") {
  cadenza::system::TimerService timer;
  timer.advanceTo(1000);
  REQUIRE(timer.start(kOwner, minutes(1)) ==
          cadenza::system::TimerRequestResult::Accepted);
  timer.advanceTo(11000);
  const auto before = timer.snapshot().remainingMs;
  CHECK_FALSE(timer.advanceTo(9000));
  CHECK(timer.snapshot().remainingMs == before);
  CHECK(timer.diagnostics().timestampRegressions == 1);
}

TEST_CASE("Large time steps expire once without callback backlog") {
  cadenza::system::TimerService timer;
  timer.advanceTo(7);
  REQUIRE(timer.start(kOwner, minutes(1)) ==
          cadenza::system::TimerRequestResult::Accepted);
  CHECK(timer.advanceTo(minutes(90)));
  for (std::uint64_t now = minutes(90); now < minutes(95); now += 1000) {
    CHECK_FALSE(timer.advanceTo(now));
  }
  CHECK(timer.snapshot().expirationGeneration == 1);
  CHECK(timer.diagnostics().expirations == 1);
}

TEST_CASE("Timer replay is deterministic for the same command and time trace") {
  auto replay = []() {
    cadenza::system::TimerService timer;
    timer.advanceTo(20);
    timer.start(kOwner, minutes(3));
    timer.advanceTo(1020);
    timer.pause(kOwner);
    timer.setRemaining(kOwner, minutes(4) + 59000);
    timer.advanceTo(50000);
    timer.resume(kOwner);
    timer.advanceTo(90000);
    return timer.snapshot();
  };
  const auto lhs = replay();
  const auto rhs = replay();
  CHECK(lhs == rhs);
}

TEST_CASE("Invalid duration state and owner requests fail closed") {
  cadenza::system::TimerService timer;
  timer.advanceTo(0);
  CHECK(timer.start(kOwner, minutes(1) - 1) ==
        cadenza::system::TimerRequestResult::InvalidDuration);
  CHECK(timer.start(kOwner, minutes(99) + 1) ==
        cadenza::system::TimerRequestResult::InvalidDuration);
  CHECK(timer.pause(kOwner) ==
        cadenza::system::TimerRequestResult::InvalidState);
  REQUIRE(timer.start(kOwner, minutes(5)) ==
          cadenza::system::TimerRequestResult::Accepted);
  CHECK(timer.pause(kOther) ==
        cadenza::system::TimerRequestResult::NotOwner);
  CHECK(timer.resume(kOwner) ==
        cadenza::system::TimerRequestResult::InvalidState);
  CHECK(timer.acknowledge() ==
        cadenza::system::TimerRequestResult::InvalidState);
}

TEST_CASE("Timer operations and time advancement allocate no memory") {
  cadenza::system::TimerService timer;
  const std::size_t before = gAllocationCount;
  timer.advanceTo(100);
  timer.start(kOwner, minutes(10));
  timer.advanceTo(200);
  timer.pause(kOwner);
  timer.setRemaining(kOwner, minutes(11));
  timer.resume(kOwner);
  timer.advanceTo(minutes(20));
  timer.acknowledge();
  CHECK(gAllocationCount == before);
}
