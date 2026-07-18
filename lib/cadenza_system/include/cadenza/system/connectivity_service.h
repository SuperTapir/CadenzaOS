#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/connectivity_types.h"
#include "cadenza/core/app_context.h"

namespace cadenza::system {

enum class ConnectivityActionType : std::uint8_t {
  StartWiFiRadio,
  StopWiFiRadio,
  ConnectWiFi,
  DisconnectWiFi,
  ResetCredentials,
  StartProvisioning,
  StopProvisioning,
  StartBluetoothLeRadio,
  StopBluetoothLeRadio,
  StartBluetoothLeAdvertising,
  StopBluetoothLeAdvertising,
  StartBluetoothLeScanning,
  StopBluetoothLeScanning,
};

struct ConnectivityAction {
  ConnectivityActionType type = ConnectivityActionType::StartWiFiRadio;
  std::uint32_t generation = 0;
};

// Privileged platform implementations own credentials and Security 2 secrets.
// The portable service can only request lifecycle actions by generation.
class ProvisioningPlatformPort {
 public:
  virtual ~ProvisioningPlatformPort() = default;
  virtual bool resetStoredCredentials(std::uint32_t generation) noexcept = 0;
  virtual bool startSecurity2Provisioning(std::uint32_t generation) noexcept = 0;
  virtual bool stopProvisioning(std::uint32_t generation) noexcept = 0;
};

class RetryJitterSource {
 public:
  virtual ~RetryJitterSource() = default;
  virtual std::uint32_t next(std::uint32_t upperExclusive) noexcept = 0;
};

enum class ProvisioningRequestResult : std::uint8_t {
  Started,
  Cancelled,
  Busy,
  Unauthorized,
  InvalidOwner,
  NotActive,
};

struct ConnectivityServiceConfig {
  std::uint32_t associationTimeoutMs = 10000;
  std::uint32_t dhcpTimeoutMs = 10000;
  std::uint32_t initialRetryDelayMs = 500;
  std::uint32_t maximumRetryDelayMs = 8000;
  std::uint8_t maximumAttempts = 4;
  std::uint32_t initialGeneration = 0;
  std::uint32_t provisioningTimeoutMs = 120000;
  std::uint32_t bluetoothScanTimeoutMs = 10000;
  std::uint32_t maximumRetryJitterMs = 0;
  RetryJitterSource* retryJitter = nullptr;
};

struct ConnectivityServiceDiagnostics {
  std::uint32_t staleEvents = 0;
  std::uint32_t actionOverflows = 0;
  std::uint32_t synchronousDriverFailures = 0;
  std::uint32_t provisioningBusy = 0;
  std::uint32_t provisioningUnauthorized = 0;
  std::uint32_t provisioningTimeouts = 0;
};

class ConnectivityService {
 public:
  explicit ConnectivityService(
      ConnectivityServiceConfig config = {}) noexcept;

  void setWiFiAvailable(bool available) noexcept;
  void setBluetoothLeAvailable(bool available) noexcept;
  void requestNetworkOnline(bool requested) noexcept;
  void onWiFiRadioReady() noexcept;
  void onWiFiRadioStopped() noexcept;
  void onWiFiAssociated(std::uint32_t generation) noexcept;
  void onWiFiGotIp(std::uint32_t generation) noexcept;
  void onWiFiDisconnected(std::uint32_t generation, WiFiFailure failure,
                          bool explicitDisconnect = false) noexcept;
  void onWiFiDriverFailure(std::uint32_t generation,
                           WiFiFailure failure) noexcept;
  void observeBluetoothLe(const BluetoothLeSnapshot& snapshot) noexcept;
  void requestBluetoothAdvertising(bool requested) noexcept;
  void requestBluetoothScanning(bool requested) noexcept;
  void onBluetoothLeRadioReady(std::uint32_t generation) noexcept;
  void onBluetoothLeRadioStopped(std::uint32_t generation) noexcept;
  void markWiFiResyncNeeded() noexcept { snapshot_.wifi.resyncNeeded = true; }
  void markBluetoothLeResyncNeeded() noexcept {
    snapshot_.bluetoothLe.resyncNeeded = true;
  }
  void reconcileWiFi(const WiFiSnapshot& authoritative) noexcept {
    snapshot_.wifi = authoritative;
    snapshot_.wifi.resyncNeeded = false;
  }
  void reconcileBluetoothLe(
      const BluetoothLeSnapshot& authoritative) noexcept {
    snapshot_.bluetoothLe = authoritative;
    snapshot_.bluetoothLe.resyncNeeded = false;
  }
  void failWiFiReconcile() noexcept {
    snapshot_.wifi.radio = WiFiRadioState::Degraded;
    snapshot_.wifi.station = WiFiStationState::Idle;
    snapshot_.wifi.failure = WiFiFailure::Driver;
    snapshot_.wifi.resyncNeeded = false;
  }
  void failBluetoothLeReconcile() noexcept {
    snapshot_.bluetoothLe.radio = BluetoothLeRadioState::Degraded;
    snapshot_.bluetoothLe.failure = BluetoothLeFailure::Driver;
    snapshot_.bluetoothLe.resyncNeeded = false;
  }
  ProvisioningRequestResult startProvisioning(
      ResourceOwner owner, bool resetCredentialsConfirmed = false) noexcept;
  ProvisioningRequestResult cancelProvisioning(ResourceOwner owner) noexcept;
  void onProvisioningProgress(std::uint32_t generation,
                              ProvisioningState state,
                              ProvisioningFailure failure =
                                  ProvisioningFailure::None) noexcept;
  void onProvisioningStopped(std::uint32_t generation) noexcept;
  bool takeReleasedProvisioningOwner(ResourceOwner& owner) noexcept;
  void advance(std::uint32_t elapsedMs) noexcept;
  bool tryPopAction(ConnectivityAction& action) noexcept;

  const ConnectivitySnapshot& snapshot() const noexcept { return snapshot_; }
  const ConnectivityServiceDiagnostics& diagnostics() const noexcept {
    return diagnostics_;
  }

 private:
  static constexpr std::size_t kActionCapacity = 16;
  std::uint32_t nextGeneration() noexcept;
  void beginConnect() noexcept;
  void scheduleRetry(WiFiFailure failure) noexcept;
  void pushAction(ConnectivityActionType type,
                  std::uint32_t generation = 0) noexcept;
  bool current(std::uint32_t generation) const noexcept;

  ConnectivityServiceConfig config_{};
  ConnectivitySnapshot snapshot_{};
  ConnectivityServiceDiagnostics diagnostics_{};
  std::array<ConnectivityAction, kActionCapacity> actions_{};
  std::size_t actionHead_ = 0;
  std::size_t actionTail_ = 0;
  std::size_t actionCount_ = 0;
  std::uint32_t generationCounter_ = 0;
  std::uint32_t phaseElapsedMs_ = 0;
  ResourceOwner provisioningOwner_{};
  ResourceOwner releasedProvisioningOwner_{};
  ProvisioningState provisioningTerminalState_ = ProvisioningState::Inactive;
  std::uint32_t provisioningElapsedMs_ = 0;
  bool bluetoothAdvertisingDesired_ = false;
  bool bluetoothScanningDesired_ = false;
  std::uint32_t bluetoothScanElapsedMs_ = 0;
};

}  // namespace cadenza::system
