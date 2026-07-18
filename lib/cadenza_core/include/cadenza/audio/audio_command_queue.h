#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "cadenza/audio/audio_types.h"

namespace cadenza::audio {

// One Runtime/main-thread producer and one audio consumer. The producer owns
// head_, the consumer owns tail_; neither side overwrites a published slot.
class AudioCommandQueue {
 public:
  static constexpr std::size_t kCapacity = 16;
  static constexpr std::size_t kCriticalReserve = 4;
  static constexpr std::size_t kDroppableLimit =
      kCapacity - kCriticalReserve;

  bool tryPush(const AudioCommand& command) noexcept {
    const std::uint32_t head = head_.load(std::memory_order_relaxed);
    const std::uint32_t tail = tail_.load(std::memory_order_acquire);
    const std::uint32_t used = head - tail;
    if (used >= kCapacity || (command.droppable() && used >= kDroppableLimit)) {
      overflowCount_.fetch_add(1, std::memory_order_relaxed);
      return false;
    }
    commands_[head % kCapacity] = command;
    head_.store(head + 1U, std::memory_order_release);
    return true;
  }

  bool tryPop(AudioCommand& command) noexcept {
    const std::uint32_t tail = tail_.load(std::memory_order_relaxed);
    const std::uint32_t head = head_.load(std::memory_order_acquire);
    if (tail == head) return false;
    command = commands_[tail % kCapacity];
    tail_.store(tail + 1U, std::memory_order_release);
    return true;
  }

  std::size_t sizeApprox() const noexcept {
    const std::uint32_t head = head_.load(std::memory_order_acquire);
    const std::uint32_t tail = tail_.load(std::memory_order_acquire);
    return static_cast<std::size_t>(head - tail);
  }

  std::uint32_t overflowCount() const noexcept {
    return overflowCount_.load(std::memory_order_relaxed);
  }

 private:
  std::array<AudioCommand, kCapacity> commands_{};
  alignas(64) std::atomic<std::uint32_t> head_{0};
  alignas(64) std::atomic<std::uint32_t> tail_{0};
  std::atomic<std::uint32_t> overflowCount_{0};
};

}  // namespace cadenza::audio
