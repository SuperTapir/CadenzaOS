#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/system/connectivity_service.h"
#include "cadenza/system/system_service_host.h"

namespace cadenza::host {

class HeadlessConnectivityAdapter final
    : public system::ProvisioningPlatformPort {
 public:
  static constexpr std::size_t kTraceCapacity = 64;

  explicit HeadlessConnectivityAdapter(
      system::SystemServiceHost& host) noexcept;
  void pump() noexcept;
  void setAutomaticWiFiSuccess(bool enabled) noexcept {
    automaticWiFiSuccess_ = enabled;
  }
  void setAutomaticProvisioningSuccess(bool enabled) noexcept {
    automaticProvisioningSuccess_ = enabled;
  }

  bool resetStoredCredentials(std::uint32_t generation) noexcept override;
  bool startSecurity2Provisioning(
      std::uint32_t generation) noexcept override;
  bool stopProvisioning(std::uint32_t generation) noexcept override;

  std::size_t traceSize() const noexcept { return traceSize_; }
  const system::ConnectivityAction* traceAt(std::size_t index) const noexcept {
    return index < traceSize_ ? &trace_[index] : nullptr;
  }
  std::uint32_t traceOverflows() const noexcept { return traceOverflows_; }

 private:
  void record(const system::ConnectivityAction& action) noexcept;
  void execute(const system::ConnectivityAction& action) noexcept;
  void publishBluetooth() noexcept;

  system::SystemServiceHost* host_ = nullptr;
  std::array<system::ConnectivityAction, kTraceCapacity> trace_{};
  std::size_t traceSize_ = 0;
  std::uint32_t traceOverflows_ = 0;
  std::uint32_t bluetoothGeneration_ = 0;
  bool bluetoothAdvertising_ = false;
  bool bluetoothScanning_ = false;
  bool automaticWiFiSuccess_ = true;
  bool automaticProvisioningSuccess_ = false;
};

}  // namespace cadenza::host
