#pragma once

#include <cstdint>

#include "cadenza/core/app_id.h"

namespace cadenza {

enum class AppCapability : std::uint8_t {
  SoundPlay,
  SettingsWrite,
  VoiceAnalyzer,
  ConnectivityObserve,
  NetworkAcquire,
  ProvisioningManage,
  BluetoothControl,
  DiagnosticEmit,
  TimerControl,
  Count,
};

class AppCapabilitySet {
 public:
  constexpr AppCapabilitySet() noexcept = default;
  constexpr explicit AppCapabilitySet(AppCapability capability) noexcept
      : bits_(capabilityBit(capability)) {}

  static constexpr AppCapabilitySet fromRaw(std::uint32_t bits) noexcept {
    return AppCapabilitySet{bits, RawTag{}};
  }

  constexpr bool valid() const noexcept {
    return (bits_ & ~knownMask()) == 0;
  }
  constexpr bool contains(AppCapability capability) const noexcept {
    return validCapability(capability) &&
           (bits_ & capabilityBit(capability)) != 0;
  }
  constexpr bool empty() const noexcept { return bits_ == 0; }
  constexpr std::uint32_t raw() const noexcept { return bits_; }

  friend constexpr AppCapabilitySet operator|(AppCapabilitySet lhs,
                                               AppCapabilitySet rhs) noexcept {
    return fromRaw(lhs.bits_ | rhs.bits_);
  }
  friend constexpr AppCapabilitySet operator|(AppCapabilitySet lhs,
                                               AppCapability rhs) noexcept {
    return lhs | AppCapabilitySet{rhs};
  }
  friend constexpr AppCapabilitySet operator|(AppCapability lhs,
                                               AppCapability rhs) noexcept {
    return AppCapabilitySet{lhs} | AppCapabilitySet{rhs};
  }
  friend constexpr bool operator==(AppCapabilitySet lhs,
                                   AppCapabilitySet rhs) noexcept {
    return lhs.bits_ == rhs.bits_;
  }
  friend constexpr bool operator!=(AppCapabilitySet lhs,
                                   AppCapabilitySet rhs) noexcept {
    return !(lhs == rhs);
  }

 private:
  struct RawTag {};
  constexpr AppCapabilitySet(std::uint32_t bits, RawTag) noexcept
      : bits_(bits) {}

  static constexpr bool validCapability(AppCapability capability) noexcept {
    return static_cast<std::uint8_t>(capability) <
           static_cast<std::uint8_t>(AppCapability::Count);
  }
  static constexpr std::uint32_t capabilityBit(
      AppCapability capability) noexcept {
    return validCapability(capability)
               ? (std::uint32_t{1} << static_cast<std::uint8_t>(capability))
               : 0;
  }
  static constexpr std::uint32_t knownMask() noexcept {
    return (std::uint32_t{1}
            << static_cast<std::uint8_t>(AppCapability::Count)) -
           1;
  }

  std::uint32_t bits_ = 0;
};

class AppCapabilityResolver {
 public:
  virtual ~AppCapabilityResolver() = default;
  virtual bool resolveCapabilities(AppId id,
                                   AppCapabilitySet& capabilities) const
      noexcept = 0;
};

}  // namespace cadenza
