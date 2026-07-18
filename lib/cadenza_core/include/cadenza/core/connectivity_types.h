#pragma once

#include <cstdint>

namespace cadenza {

enum class WiFiDesiredPolicy : std::uint8_t { IdleAllowed, OnlineRequested };
enum class WiFiRadioState : std::uint8_t {
  Unavailable,
  Off,
  Starting,
  Ready,
  Stopping,
  Degraded,
};
enum class WiFiStationState : std::uint8_t {
  Idle,
  Connecting,
  ObtainingAddress,
  Online,
  RetryWait,
  NeedsUserAction,
};
enum class WiFiFailure : std::uint8_t {
  None,
  NoCredentials,
  AccessPointNotFound,
  AuthenticationFailed,
  AssociationTimeout,
  DhcpTimeout,
  Driver,
  RetryExhausted,
};

struct WiFiSnapshot {
  WiFiDesiredPolicy desired = WiFiDesiredPolicy::IdleAllowed;
  WiFiRadioState radio = WiFiRadioState::Unavailable;
  WiFiStationState station = WiFiStationState::Idle;
  WiFiFailure failure = WiFiFailure::None;
  std::uint32_t generation = 0;
  std::uint32_t retryDelayMs = 0;
  std::uint8_t retryAttempt = 0;
  bool resyncNeeded = false;

  constexpr bool networkOnline() const noexcept {
    return radio == WiFiRadioState::Ready &&
           station == WiFiStationState::Online;
  }
  constexpr bool needsUserAction() const noexcept {
    return station == WiFiStationState::NeedsUserAction;
  }
};

enum class BluetoothLeRadioState : std::uint8_t {
  Unavailable,
  Off,
  Starting,
  Ready,
  Stopping,
  Degraded,
};
enum class BluetoothLeFailure : std::uint8_t {
  None,
  Driver,
  Timeout,
  CapacityExceeded,
};

struct BluetoothLeSnapshot {
  BluetoothLeRadioState radio = BluetoothLeRadioState::Unavailable;
  BluetoothLeFailure failure = BluetoothLeFailure::None;
  std::uint32_t generation = 0;
  std::uint8_t connectionCount = 0;
  bool advertising = false;
  bool scanning = false;
  bool provisioningRole = false;
  bool resyncNeeded = false;
};

enum class ProvisioningState : std::uint8_t {
  Inactive,
  Advertising,
  Negotiating,
  Applying,
  Verifying,
  Succeeded,
  Failed,
  Stopping,
};
enum class ProvisioningFailure : std::uint8_t {
  None,
  Unauthorized,
  Busy,
  Timeout,
  AuthenticationFailed,
  AccessPointNotFound,
  Transport,
  Cancelled,
};
struct ProvisioningSnapshot {
  ProvisioningState state = ProvisioningState::Inactive;
  ProvisioningFailure failure = ProvisioningFailure::None;
  std::uint32_t generation = 0;
};

struct ConnectivitySummary {
  bool networkOnline = false;
  bool networkNeedsUserAction = false;
  bool bluetoothReady = false;
  bool bluetoothPeerConnected = false;
  bool provisioningActive = false;
};

struct ConnectivitySnapshot {
  WiFiSnapshot wifi{};
  BluetoothLeSnapshot bluetoothLe{};
  ProvisioningSnapshot provisioning{};

  constexpr ConnectivitySummary summary() const noexcept {
    return {wifi.networkOnline(), wifi.needsUserAction(),
            bluetoothLe.radio == BluetoothLeRadioState::Ready,
            bluetoothLe.connectionCount != 0,
            provisioning.state != ProvisioningState::Inactive &&
                provisioning.state != ProvisioningState::Succeeded &&
                provisioning.state != ProvisioningState::Failed};
  }
};

}  // namespace cadenza
