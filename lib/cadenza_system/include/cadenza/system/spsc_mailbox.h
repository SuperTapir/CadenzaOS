#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace cadenza::system {

struct SpscMailboxDiagnostics {
  std::uint32_t posted = 0;
  std::uint32_t consumed = 0;
  std::uint32_t rejected = 0;
  std::size_t highWater = 0;
};

// A bounded queue for exactly one producer execution context and exactly one
// consumer execution context. Full queues reject the newest value so an unread
// slot is never overwritten. Index is configurable only to exercise modular
// wraparound in host tests; production uses uint32_t.
template <typename Event, std::size_t Capacity,
          typename Index = std::uint32_t>
class SpscMailbox {
 public:
  static_assert(Capacity > 0, "SPSC mailbox cannot be empty");
  static_assert(std::is_integral_v<Index> && std::is_unsigned_v<Index>,
                "SPSC index must be an unsigned integer");
  static_assert(Capacity <
                    (std::uintmax_t{1}
                     << (std::numeric_limits<Index>::digits - 1)),
                "Capacity must be below half the modular index range");
  static_assert(std::is_trivially_copyable_v<Event>,
                "Callback events must be trivially copyable");

  SpscMailbox() noexcept = default;
  SpscMailbox(const SpscMailbox&) = delete;
  SpscMailbox& operator=(const SpscMailbox&) = delete;

  bool postFromProducer(const Event& event) noexcept {
    const Index write = writeIndex_.load(std::memory_order_relaxed);
    const Index read = readIndex_.load(std::memory_order_acquire);
    const Index distance = static_cast<Index>(write - read);
    if (static_cast<std::size_t>(distance) >= Capacity) {
      rejected_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    events_[static_cast<std::size_t>(write) % Capacity] = event;
    writeIndex_.store(static_cast<Index>(write + Index{1}),
                      std::memory_order_release);
    posted_.fetch_add(1, std::memory_order_relaxed);
    updateHighWater(static_cast<std::size_t>(distance) + 1);
    return true;
  }

  bool postFromCallback(const Event& event) noexcept {
    return postFromProducer(event);
  }

  bool tryPop(Event& event) noexcept {
    const Index read = readIndex_.load(std::memory_order_relaxed);
    const Index write = writeIndex_.load(std::memory_order_acquire);
    if (read == write) return false;
    event = events_[static_cast<std::size_t>(read) % Capacity];
    readIndex_.store(static_cast<Index>(read + Index{1}),
                     std::memory_order_release);
    consumed_.fetch_add(1, std::memory_order_relaxed);
    return true;
  }

  SpscMailboxDiagnostics diagnostics() const noexcept {
    return {posted_.load(std::memory_order_relaxed),
            consumed_.load(std::memory_order_relaxed),
            rejected_.load(std::memory_order_relaxed),
            highWater_.load(std::memory_order_relaxed)};
  }

 private:
  void updateHighWater(std::size_t value) noexcept {
    std::size_t observed = highWater_.load(std::memory_order_relaxed);
    while (observed < value &&
           !highWater_.compare_exchange_weak(observed, value,
                                             std::memory_order_relaxed,
                                             std::memory_order_relaxed)) {
    }
  }

  std::array<Event, Capacity> events_{};
  alignas(64) std::atomic<Index> writeIndex_{0};
  alignas(64) std::atomic<Index> readIndex_{0};
  std::atomic<std::uint32_t> posted_{0};
  std::atomic<std::uint32_t> consumed_{0};
  std::atomic<std::uint32_t> rejected_{0};
  std::atomic<std::size_t> highWater_{0};
};

}  // namespace cadenza::system
