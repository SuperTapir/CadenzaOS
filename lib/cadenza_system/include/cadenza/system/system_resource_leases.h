#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/app_context.h"

namespace cadenza::system {

enum class SystemResource : std::uint8_t {
  Network,
  BluetoothAdvertising,
  BluetoothScanning,
  VoiceAnalyzer,
  ProvisioningSession,
  Count,
};

enum class LeaseLifetime : std::uint8_t { Foreground, Session, Persistent };

enum class LeaseAcquireResult : std::uint8_t {
  Acquired,
  AlreadyHeld,
  InvalidOwner,
  PersistentDenied,
  CapacityExceeded,
};

enum class LeaseReleaseResult : std::uint8_t { Released, NotFound };

struct SystemResourceLease {
  ResourceOwner owner{};
  SystemResource resource = SystemResource::Network;
  LeaseLifetime lifetime = LeaseLifetime::Foreground;
  std::uint32_t generation = 0;
  bool occupied = false;
};

struct SystemResourceLeaseDiagnostics {
  std::uint32_t acquired = 0;
  std::uint32_t duplicateAcquires = 0;
  std::uint32_t released = 0;
  std::uint32_t unknownReleases = 0;
  std::uint32_t deniedPersistent = 0;
  std::uint32_t capacityFailures = 0;
  std::uint32_t autoReleased = 0;
  std::size_t highWater = 0;
};

class SystemResourceLeaseTable {
 public:
  static constexpr std::size_t kCapacity = 16;

  LeaseAcquireResult acquire(ResourceOwner owner, SystemResource resource,
                             LeaseLifetime lifetime,
                             std::uint32_t generation = 0) noexcept;
  LeaseReleaseResult release(ResourceOwner owner,
                             SystemResource resource) noexcept;
  std::size_t releaseForeground(ResourceOwner owner) noexcept;
  std::size_t ownerCount(SystemResource resource) const noexcept;
  bool desired(SystemResource resource) const noexcept {
    return ownerCount(resource) != 0;
  }
  std::size_t size() const noexcept { return size_; }
  const SystemResourceLeaseDiagnostics& diagnostics() const noexcept {
    return diagnostics_;
  }

 private:
  SystemResourceLease* find(ResourceOwner owner,
                            SystemResource resource) noexcept;
  std::array<SystemResourceLease, kCapacity> leases_{};
  std::size_t size_ = 0;
  SystemResourceLeaseDiagnostics diagnostics_{};
};

}  // namespace cadenza::system
