#pragma once

#include <cstdint>

namespace cadenza {

class AppId {
 public:
  using Value = std::uint16_t;
  static constexpr Value kInvalidValue = 0xFFFF;

  constexpr AppId() noexcept = default;
  explicit constexpr AppId(Value value) noexcept : value_(value) {}

  constexpr bool valid() const noexcept { return value_ != kInvalidValue; }
  constexpr Value value() const noexcept { return value_; }

  friend constexpr bool operator==(AppId lhs, AppId rhs) noexcept {
    return lhs.value_ == rhs.value_;
  }
  friend constexpr bool operator!=(AppId lhs, AppId rhs) noexcept {
    return !(lhs == rhs);
  }

 private:
  Value value_ = kInvalidValue;
};

}  // namespace cadenza
