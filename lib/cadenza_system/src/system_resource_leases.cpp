#include "cadenza/system/system_resource_leases.h"

#include <algorithm>

namespace cadenza::system {

SystemResourceLease* SystemResourceLeaseTable::find(
    ResourceOwner owner, SystemResource resource) noexcept {
  for (auto& lease : leases_) {
    if (lease.occupied && lease.owner == owner && lease.resource == resource) {
      return &lease;
    }
  }
  return nullptr;
}

LeaseAcquireResult SystemResourceLeaseTable::acquire(
    ResourceOwner owner, SystemResource resource, LeaseLifetime lifetime,
    std::uint32_t generation) noexcept {
  if (!owner.valid() || resource == SystemResource::Count) {
    return LeaseAcquireResult::InvalidOwner;
  }
  if (lifetime == LeaseLifetime::Persistent &&
      owner.kind == ResourceOwnerKind::App) {
    ++diagnostics_.deniedPersistent;
    return LeaseAcquireResult::PersistentDenied;
  }
  if (SystemResourceLease* existing = find(owner, resource)) {
    existing->lifetime = lifetime;
    existing->generation = generation;
    ++diagnostics_.duplicateAcquires;
    return LeaseAcquireResult::AlreadyHeld;
  }
  for (auto& lease : leases_) {
    if (lease.occupied) continue;
    lease = {owner, resource, lifetime, generation, true};
    ++size_;
    ++diagnostics_.acquired;
    diagnostics_.highWater = std::max(diagnostics_.highWater, size_);
    return LeaseAcquireResult::Acquired;
  }
  ++diagnostics_.capacityFailures;
  return LeaseAcquireResult::CapacityExceeded;
}

LeaseReleaseResult SystemResourceLeaseTable::release(
    ResourceOwner owner, SystemResource resource) noexcept {
  SystemResourceLease* lease = find(owner, resource);
  if (!lease) {
    ++diagnostics_.unknownReleases;
    return LeaseReleaseResult::NotFound;
  }
  *lease = {};
  --size_;
  ++diagnostics_.released;
  return LeaseReleaseResult::Released;
}

std::size_t SystemResourceLeaseTable::releaseForeground(
    ResourceOwner owner) noexcept {
  std::size_t released = 0;
  for (auto& lease : leases_) {
    if (!lease.occupied || !(lease.owner == owner) ||
        lease.lifetime != LeaseLifetime::Foreground) {
      continue;
    }
    lease = {};
    --size_;
    ++released;
  }
  diagnostics_.released += static_cast<std::uint32_t>(released);
  diagnostics_.autoReleased += static_cast<std::uint32_t>(released);
  return released;
}

std::size_t SystemResourceLeaseTable::ownerCount(
    SystemResource resource) const noexcept {
  std::size_t count = 0;
  for (const auto& lease : leases_) {
    if (lease.occupied && lease.resource == resource) ++count;
  }
  return count;
}

}  // namespace cadenza::system
