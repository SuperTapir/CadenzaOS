#include "cadenza/host/headless_connectivity_adapter.h"

namespace cadenza::host {

HeadlessConnectivityAdapter::HeadlessConnectivityAdapter(
    system::SystemServiceHost& host) noexcept
    : host_(&host) {
  host_->postPlatformEvent(system::PlatformEvent::wifiAvailability(true));
  host_->postPlatformEvent(system::PlatformEvent::bluetoothAvailability(true));
}

void HeadlessConnectivityAdapter::record(
    const system::ConnectivityAction& action) noexcept {
  if (traceSize_ < trace_.size()) {
    trace_[traceSize_++] = action;
  } else {
    ++traceOverflows_;
  }
}

void HeadlessConnectivityAdapter::pump() noexcept {
  system::ConnectivityAction action;
  while (host_->connectivityService().tryPopAction(action)) {
    record(action);
    execute(action);
  }
}

bool HeadlessConnectivityAdapter::resetStoredCredentials(
    std::uint32_t) noexcept {
  return true;
}

bool HeadlessConnectivityAdapter::startSecurity2Provisioning(
    std::uint32_t generation) noexcept {
  host_->postPlatformEvent(system::PlatformEvent::provisioningProgress(
      generation, ProvisioningState::Negotiating));
  if (automaticProvisioningSuccess_) {
    host_->postPlatformEvent(system::PlatformEvent::provisioningProgress(
        generation, ProvisioningState::Succeeded));
  }
  return true;
}

bool HeadlessConnectivityAdapter::stopProvisioning(
    std::uint32_t generation) noexcept {
  return host_->postPlatformEvent(
      system::PlatformEvent::provisioningStopped(generation));
}

void HeadlessConnectivityAdapter::publishBluetooth() noexcept {
  BluetoothLeSnapshot snapshot;
  snapshot.radio = BluetoothLeRadioState::Ready;
  snapshot.generation = bluetoothGeneration_;
  snapshot.advertising = bluetoothAdvertising_;
  snapshot.scanning = bluetoothScanning_;
  host_->postPlatformEvent(system::PlatformEvent::bluetoothObserved(snapshot));
}

void HeadlessConnectivityAdapter::execute(
    const system::ConnectivityAction& action) noexcept {
  using system::ConnectivityActionType;
  switch (action.type) {
    case ConnectivityActionType::StartWiFiRadio:
      host_->postPlatformEvent(system::PlatformEvent::wifiRadioReady());
      break;
    case ConnectivityActionType::StopWiFiRadio:
      host_->postPlatformEvent(system::PlatformEvent::wifiRadioStopped());
      break;
    case ConnectivityActionType::ConnectWiFi:
      if (automaticWiFiSuccess_) {
        host_->postPlatformEvent(
            system::PlatformEvent::wifiAssociated(action.generation));
        host_->postPlatformEvent(
            system::PlatformEvent::wifiGotIp(action.generation));
      }
      break;
    case ConnectivityActionType::DisconnectWiFi:
      host_->postPlatformEvent(system::PlatformEvent::wifiDisconnected(
          action.generation, WiFiFailure::None));
      break;
    case ConnectivityActionType::ResetCredentials:
      resetStoredCredentials(action.generation);
      break;
    case ConnectivityActionType::StartProvisioning:
      startSecurity2Provisioning(action.generation);
      break;
    case ConnectivityActionType::StopProvisioning:
      stopProvisioning(action.generation);
      break;
    case ConnectivityActionType::StartBluetoothLeRadio:
      bluetoothGeneration_ = action.generation;
      host_->postPlatformEvent(
          system::PlatformEvent::bluetoothRadioReady(action.generation));
      break;
    case ConnectivityActionType::StopBluetoothLeRadio:
      bluetoothAdvertising_ = false;
      bluetoothScanning_ = false;
      host_->postPlatformEvent(
          system::PlatformEvent::bluetoothRadioStopped(action.generation));
      break;
    case ConnectivityActionType::StartBluetoothLeAdvertising:
      bluetoothAdvertising_ = true;
      publishBluetooth();
      break;
    case ConnectivityActionType::StopBluetoothLeAdvertising:
      bluetoothAdvertising_ = false;
      publishBluetooth();
      break;
    case ConnectivityActionType::StartBluetoothLeScanning:
      bluetoothScanning_ = true;
      publishBluetooth();
      break;
    case ConnectivityActionType::StopBluetoothLeScanning:
      bluetoothScanning_ = false;
      publishBluetooth();
      break;
  }
}

}  // namespace cadenza::host
