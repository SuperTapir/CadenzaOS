#pragma once

#include <array>
#include <cstddef>

#include "cadenza/core/app_capability.h"
#include "cadenza/core/app_id.h"

namespace cadenza {

class App;

enum class AppRegistrationResult {
  Registered,
  InvalidId,
  DuplicateId,
  UnknownCapabilities,
  CapacityExceeded,
};

struct AppCatalogEntry {
  AppId id{};
  App* app = nullptr;
  const char* name = "MISSING";
  bool visibleInLauncher = false;
  AppCapabilitySet capabilities{};
};

struct AppDescriptor {
  AppId id{};
  App* app = nullptr;
  const char* name = "MISSING";
  bool visibleInLauncher = false;
  AppCapabilitySet capabilities{};
};

class AppCatalog {
 public:
  static constexpr std::size_t kMaxCapacity = 16;

  explicit constexpr AppCatalog(
      std::size_t capacity = kMaxCapacity) noexcept
      : capacity_(capacity <= kMaxCapacity ? capacity : kMaxCapacity) {}

  AppRegistrationResult registerApp(AppId id, App& app,
                                    bool visibleInLauncher,
                                    const char* name = "MISSING",
                                    AppCapabilitySet capabilities = {}) noexcept {
    if (!id.valid()) return AppRegistrationResult::InvalidId;
    if (!capabilities.valid()) {
      return AppRegistrationResult::UnknownCapabilities;
    }
    if (find(id)) return AppRegistrationResult::DuplicateId;
    if (size_ >= capacity_) return AppRegistrationResult::CapacityExceeded;
    entries_[size_++] = {id, &app, name ? name : "MISSING",
                         visibleInLauncher, capabilities};
    return AppRegistrationResult::Registered;
  }

  AppRegistrationResult registerApp(const AppDescriptor& descriptor) noexcept {
    if (!descriptor.app) return AppRegistrationResult::InvalidId;
    return registerApp(descriptor.id, *descriptor.app,
                       descriptor.visibleInLauncher, descriptor.name,
                       descriptor.capabilities);
  }

  constexpr std::size_t size() const noexcept { return size_; }
  constexpr std::size_t capacity() const noexcept { return capacity_; }

  const AppCatalogEntry* at(std::size_t position) const noexcept {
    return position < size_ ? &entries_[position] : nullptr;
  }

  AppCatalogEntry* find(AppId id) noexcept {
    for (std::size_t index = 0; index < size_; ++index) {
      if (entries_[index].id == id) return &entries_[index];
    }
    return nullptr;
  }

  const AppCatalogEntry* find(AppId id) const noexcept {
    for (std::size_t index = 0; index < size_; ++index) {
      if (entries_[index].id == id) return &entries_[index];
    }
    return nullptr;
  }

 private:
  std::array<AppCatalogEntry, kMaxCapacity> entries_{};
  std::size_t capacity_ = kMaxCapacity;
  std::size_t size_ = 0;
};

}  // namespace cadenza
