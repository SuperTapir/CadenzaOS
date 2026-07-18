#include "cadenza/system/connectivity_service.h"

#include <algorithm>

namespace cadenza::system {

ConnectivityService::ConnectivityService(
    ConnectivityServiceConfig config) noexcept
    : config_(config), generationCounter_(config.initialGeneration) {
  if (config_.maximumAttempts == 0) config_.maximumAttempts = 1;
  if (config_.maximumRetryDelayMs < config_.initialRetryDelayMs) {
    config_.maximumRetryDelayMs = config_.initialRetryDelayMs;
  }
}

void ConnectivityService::pushAction(ConnectivityActionType type,
                                     std::uint32_t generation) noexcept {
  if (actionCount_ == kActionCapacity) {
    ++diagnostics_.actionOverflows;
    snapshot_.wifi.resyncNeeded = true;
    return;
  }
  actions_[actionTail_] = {type, generation};
  actionTail_ = (actionTail_ + 1) % kActionCapacity;
  ++actionCount_;
}

bool ConnectivityService::tryPopAction(ConnectivityAction& action) noexcept {
  if (actionCount_ == 0) return false;
  action = actions_[actionHead_];
  actionHead_ = (actionHead_ + 1) % kActionCapacity;
  --actionCount_;
  return true;
}

std::uint32_t ConnectivityService::nextGeneration() noexcept {
  ++generationCounter_;
  if (generationCounter_ == 0) ++generationCounter_;
  return generationCounter_;
}

bool ConnectivityService::current(std::uint32_t generation) const noexcept {
  return generation != 0 && generation == snapshot_.wifi.generation;
}

void ConnectivityService::setWiFiAvailable(bool available) noexcept {
  if (!available) {
    snapshot_.wifi = {};
    return;
  }
  if (snapshot_.wifi.radio == WiFiRadioState::Unavailable ||
      snapshot_.wifi.radio == WiFiRadioState::Stopping) {
    snapshot_.wifi.radio = WiFiRadioState::Off;
  }
}

void ConnectivityService::setBluetoothLeAvailable(bool available) noexcept {
  if (!available) {
    snapshot_.bluetoothLe = {};
    bluetoothAdvertisingDesired_ = false;
    bluetoothScanningDesired_ = false;
    return;
  }
  if (snapshot_.bluetoothLe.radio == BluetoothLeRadioState::Unavailable ||
      snapshot_.bluetoothLe.radio == BluetoothLeRadioState::Stopping) {
    snapshot_.bluetoothLe.radio = BluetoothLeRadioState::Off;
  }
}

void ConnectivityService::requestNetworkOnline(bool requested) noexcept {
  snapshot_.wifi.desired = requested ? WiFiDesiredPolicy::OnlineRequested
                                     : WiFiDesiredPolicy::IdleAllowed;
  if (!requested) {
    snapshot_.wifi.retryDelayMs = 0;
    snapshot_.wifi.retryAttempt = 0;
    if (snapshot_.wifi.station != WiFiStationState::Idle) {
      pushAction(ConnectivityActionType::DisconnectWiFi,
                 snapshot_.wifi.generation);
      snapshot_.wifi.station = WiFiStationState::Idle;
    }
    if (snapshot_.wifi.radio == WiFiRadioState::Starting ||
        snapshot_.wifi.radio == WiFiRadioState::Ready) {
      pushAction(ConnectivityActionType::StopWiFiRadio);
      snapshot_.wifi.radio = WiFiRadioState::Stopping;
    }
    return;
  }
  if (snapshot_.wifi.radio == WiFiRadioState::Off) {
    snapshot_.wifi.radio = WiFiRadioState::Starting;
    pushAction(ConnectivityActionType::StartWiFiRadio);
  } else if (snapshot_.wifi.radio == WiFiRadioState::Ready &&
             snapshot_.wifi.station == WiFiStationState::Idle) {
    beginConnect();
  }
}

void ConnectivityService::onWiFiRadioReady() noexcept {
  snapshot_.wifi.radio = WiFiRadioState::Ready;
  if (snapshot_.wifi.desired == WiFiDesiredPolicy::OnlineRequested &&
      snapshot_.wifi.station == WiFiStationState::Idle) {
    beginConnect();
  }
}

void ConnectivityService::onWiFiRadioStopped() noexcept {
  snapshot_.wifi.radio = WiFiRadioState::Off;
  snapshot_.wifi.station = WiFiStationState::Idle;
}

void ConnectivityService::beginConnect() noexcept {
  snapshot_.wifi.generation = nextGeneration();
  snapshot_.wifi.station = WiFiStationState::Connecting;
  snapshot_.wifi.failure = WiFiFailure::None;
  snapshot_.wifi.retryDelayMs = 0;
  phaseElapsedMs_ = 0;
  pushAction(ConnectivityActionType::ConnectWiFi,
             snapshot_.wifi.generation);
}

void ConnectivityService::onWiFiAssociated(
    std::uint32_t generation) noexcept {
  if (!current(generation)) {
    ++diagnostics_.staleEvents;
    return;
  }
  snapshot_.wifi.station = WiFiStationState::ObtainingAddress;
  snapshot_.wifi.failure = WiFiFailure::None;
  phaseElapsedMs_ = 0;
}

void ConnectivityService::onWiFiGotIp(std::uint32_t generation) noexcept {
  if (!current(generation) ||
      snapshot_.wifi.station != WiFiStationState::ObtainingAddress) {
    ++diagnostics_.staleEvents;
    return;
  }
  snapshot_.wifi.station = WiFiStationState::Online;
  snapshot_.wifi.failure = WiFiFailure::None;
  snapshot_.wifi.retryAttempt = 0;
  phaseElapsedMs_ = 0;
}

void ConnectivityService::scheduleRetry(WiFiFailure failure) noexcept {
  snapshot_.wifi.failure = failure;
  if (failure == WiFiFailure::AuthenticationFailed ||
      failure == WiFiFailure::NoCredentials) {
    snapshot_.wifi.station = WiFiStationState::NeedsUserAction;
    snapshot_.wifi.retryDelayMs = 0;
    return;
  }
  if (snapshot_.wifi.retryAttempt >= config_.maximumAttempts) {
    snapshot_.wifi.failure = WiFiFailure::RetryExhausted;
    snapshot_.wifi.station = WiFiStationState::NeedsUserAction;
    snapshot_.wifi.retryDelayMs = 0;
    return;
  }
  ++snapshot_.wifi.retryAttempt;
  std::uint32_t delay = config_.initialRetryDelayMs;
  for (std::uint8_t attempt = 1; attempt < snapshot_.wifi.retryAttempt;
       ++attempt) {
    delay = std::min(config_.maximumRetryDelayMs, delay * 2U);
  }
  if (config_.retryJitter && config_.maximumRetryJitterMs != 0) {
    const std::uint32_t jitter = config_.retryJitter->next(
        config_.maximumRetryJitterMs + 1U);
    const std::uint32_t remaining = config_.maximumRetryDelayMs - delay;
    delay += std::min(remaining, jitter);
  }
  snapshot_.wifi.retryDelayMs = delay;
  snapshot_.wifi.station = WiFiStationState::RetryWait;
  phaseElapsedMs_ = 0;
}

void ConnectivityService::onWiFiDisconnected(
    std::uint32_t generation, WiFiFailure failure,
    bool explicitDisconnect) noexcept {
  if (!current(generation)) {
    ++diagnostics_.staleEvents;
    return;
  }
  if (explicitDisconnect ||
      snapshot_.wifi.desired == WiFiDesiredPolicy::IdleAllowed) {
    snapshot_.wifi.station = WiFiStationState::Idle;
    snapshot_.wifi.failure = failure;
    return;
  }
  scheduleRetry(failure);
}

void ConnectivityService::onWiFiDriverFailure(
    std::uint32_t generation, WiFiFailure failure) noexcept {
  if (!current(generation)) {
    ++diagnostics_.staleEvents;
    return;
  }
  ++diagnostics_.synchronousDriverFailures;
  scheduleRetry(failure == WiFiFailure::None ? WiFiFailure::Driver : failure);
}

ProvisioningRequestResult ConnectivityService::startProvisioning(
    ResourceOwner owner, bool resetCredentialsConfirmed) noexcept {
  if (!owner.valid()) return ProvisioningRequestResult::InvalidOwner;
  if (snapshot_.provisioning.state != ProvisioningState::Inactive &&
      snapshot_.provisioning.state != ProvisioningState::Succeeded &&
      snapshot_.provisioning.state != ProvisioningState::Failed) {
    ++diagnostics_.provisioningBusy;
    return ProvisioningRequestResult::Busy;
  }
  provisioningOwner_ = owner;
  snapshot_.provisioning.generation = nextGeneration();
  snapshot_.provisioning.state = ProvisioningState::Advertising;
  snapshot_.provisioning.failure = ProvisioningFailure::None;
  provisioningTerminalState_ = ProvisioningState::Inactive;
  provisioningElapsedMs_ = 0;
  if (resetCredentialsConfirmed) {
    pushAction(ConnectivityActionType::ResetCredentials,
               snapshot_.provisioning.generation);
  }
  pushAction(ConnectivityActionType::StartProvisioning,
             snapshot_.provisioning.generation);
  return ProvisioningRequestResult::Started;
}

ProvisioningRequestResult ConnectivityService::cancelProvisioning(
    ResourceOwner owner) noexcept {
  if (!owner.valid()) return ProvisioningRequestResult::InvalidOwner;
  if (!provisioningOwner_.valid()) return ProvisioningRequestResult::NotActive;
  if (!(owner == provisioningOwner_)) {
    ++diagnostics_.provisioningUnauthorized;
    return ProvisioningRequestResult::Unauthorized;
  }
  snapshot_.provisioning.state = ProvisioningState::Stopping;
  snapshot_.provisioning.failure = ProvisioningFailure::Cancelled;
  provisioningTerminalState_ = ProvisioningState::Failed;
  releasedProvisioningOwner_ = provisioningOwner_;
  pushAction(ConnectivityActionType::StopProvisioning,
             snapshot_.provisioning.generation);
  return ProvisioningRequestResult::Cancelled;
}

void ConnectivityService::onProvisioningProgress(
    std::uint32_t generation, ProvisioningState state,
    ProvisioningFailure failure) noexcept {
  if (generation == 0 || generation != snapshot_.provisioning.generation ||
      !provisioningOwner_.valid()) {
    ++diagnostics_.staleEvents;
    return;
  }
  snapshot_.provisioning.state = state;
  snapshot_.provisioning.failure = failure;
  provisioningElapsedMs_ = 0;
  if (state == ProvisioningState::Succeeded ||
      state == ProvisioningState::Failed) {
    provisioningTerminalState_ = state;
    releasedProvisioningOwner_ = provisioningOwner_;
    snapshot_.provisioning.state = ProvisioningState::Stopping;
    pushAction(ConnectivityActionType::StopProvisioning, generation);
  }
}

void ConnectivityService::onProvisioningStopped(
    std::uint32_t generation) noexcept {
  if (generation == 0 || generation != snapshot_.provisioning.generation ||
      !provisioningOwner_.valid()) {
    ++diagnostics_.staleEvents;
    return;
  }
  snapshot_.provisioning.state = provisioningTerminalState_;
  provisioningOwner_ = {};
  provisioningElapsedMs_ = 0;
}

bool ConnectivityService::takeReleasedProvisioningOwner(
    ResourceOwner& owner) noexcept {
  if (!releasedProvisioningOwner_.valid()) return false;
  owner = releasedProvisioningOwner_;
  releasedProvisioningOwner_ = {};
  return true;
}

void ConnectivityService::requestBluetoothAdvertising(bool requested) noexcept {
  if (requested == bluetoothAdvertisingDesired_) return;
  bluetoothAdvertisingDesired_ = requested;
  if (requested) {
    if (snapshot_.bluetoothLe.radio == BluetoothLeRadioState::Off) {
      snapshot_.bluetoothLe.generation = nextGeneration();
      snapshot_.bluetoothLe.radio = BluetoothLeRadioState::Starting;
      pushAction(ConnectivityActionType::StartBluetoothLeRadio,
                 snapshot_.bluetoothLe.generation);
    } else if (snapshot_.bluetoothLe.radio == BluetoothLeRadioState::Ready) {
      pushAction(ConnectivityActionType::StartBluetoothLeAdvertising,
                 snapshot_.bluetoothLe.generation);
    }
  } else {
    pushAction(ConnectivityActionType::StopBluetoothLeAdvertising,
               snapshot_.bluetoothLe.generation);
    if (!bluetoothScanningDesired_) {
      snapshot_.bluetoothLe.radio = BluetoothLeRadioState::Stopping;
      pushAction(ConnectivityActionType::StopBluetoothLeRadio,
                 snapshot_.bluetoothLe.generation);
    }
  }
}

void ConnectivityService::requestBluetoothScanning(bool requested) noexcept {
  if (requested == bluetoothScanningDesired_) return;
  bluetoothScanningDesired_ = requested;
  bluetoothScanElapsedMs_ = 0;
  if (requested) {
    snapshot_.bluetoothLe.failure = BluetoothLeFailure::None;
    if (snapshot_.bluetoothLe.radio == BluetoothLeRadioState::Off) {
      snapshot_.bluetoothLe.generation = nextGeneration();
      snapshot_.bluetoothLe.radio = BluetoothLeRadioState::Starting;
      pushAction(ConnectivityActionType::StartBluetoothLeRadio,
                 snapshot_.bluetoothLe.generation);
    } else if (snapshot_.bluetoothLe.radio == BluetoothLeRadioState::Ready) {
      pushAction(ConnectivityActionType::StartBluetoothLeScanning,
                 snapshot_.bluetoothLe.generation);
    }
  } else {
    pushAction(ConnectivityActionType::StopBluetoothLeScanning,
               snapshot_.bluetoothLe.generation);
    if (!bluetoothAdvertisingDesired_) {
      snapshot_.bluetoothLe.radio = BluetoothLeRadioState::Stopping;
      pushAction(ConnectivityActionType::StopBluetoothLeRadio,
                 snapshot_.bluetoothLe.generation);
    }
  }
}

void ConnectivityService::onBluetoothLeRadioReady(
    std::uint32_t generation) noexcept {
  if (generation == 0 || generation != snapshot_.bluetoothLe.generation) {
    ++diagnostics_.staleEvents;
    return;
  }
  snapshot_.bluetoothLe.radio = BluetoothLeRadioState::Ready;
  if (bluetoothAdvertisingDesired_) {
    pushAction(ConnectivityActionType::StartBluetoothLeAdvertising,
               generation);
  }
  if (bluetoothScanningDesired_) {
    pushAction(ConnectivityActionType::StartBluetoothLeScanning, generation);
  }
}

void ConnectivityService::onBluetoothLeRadioStopped(
    std::uint32_t generation) noexcept {
  if (generation == 0 || generation != snapshot_.bluetoothLe.generation) {
    ++diagnostics_.staleEvents;
    return;
  }
  snapshot_.bluetoothLe.radio = BluetoothLeRadioState::Off;
  snapshot_.bluetoothLe.advertising = false;
  snapshot_.bluetoothLe.scanning = false;
}

void ConnectivityService::observeBluetoothLe(
    const BluetoothLeSnapshot& snapshot) noexcept {
  if (snapshot.generation == 0 ||
      snapshot.generation != snapshot_.bluetoothLe.generation) {
    ++diagnostics_.staleEvents;
    return;
  }
  const bool resyncNeeded = snapshot_.bluetoothLe.resyncNeeded;
  snapshot_.bluetoothLe = snapshot;
  snapshot_.bluetoothLe.resyncNeeded = resyncNeeded;
}

void ConnectivityService::advance(std::uint32_t elapsedMs) noexcept {
  phaseElapsedMs_ += elapsedMs;
  if (provisioningOwner_.valid() &&
      snapshot_.provisioning.state != ProvisioningState::Stopping) {
    provisioningElapsedMs_ += elapsedMs;
    if (provisioningElapsedMs_ >= config_.provisioningTimeoutMs) {
      ++diagnostics_.provisioningTimeouts;
      snapshot_.provisioning.state = ProvisioningState::Stopping;
      snapshot_.provisioning.failure = ProvisioningFailure::Timeout;
      provisioningTerminalState_ = ProvisioningState::Failed;
      releasedProvisioningOwner_ = provisioningOwner_;
      pushAction(ConnectivityActionType::StopProvisioning,
                 snapshot_.provisioning.generation);
    }
  }
  if (bluetoothScanningDesired_) {
    bluetoothScanElapsedMs_ += elapsedMs;
    if (bluetoothScanElapsedMs_ >= config_.bluetoothScanTimeoutMs) {
      bluetoothScanningDesired_ = false;
      snapshot_.bluetoothLe.failure = BluetoothLeFailure::Timeout;
      snapshot_.bluetoothLe.scanning = false;
      pushAction(ConnectivityActionType::StopBluetoothLeScanning,
                 snapshot_.bluetoothLe.generation);
      if (!bluetoothAdvertisingDesired_) {
        snapshot_.bluetoothLe.radio = BluetoothLeRadioState::Stopping;
        pushAction(ConnectivityActionType::StopBluetoothLeRadio,
                   snapshot_.bluetoothLe.generation);
      }
    }
  }
  if (snapshot_.wifi.station == WiFiStationState::Connecting &&
      phaseElapsedMs_ >= config_.associationTimeoutMs) {
    scheduleRetry(WiFiFailure::AssociationTimeout);
  } else if (snapshot_.wifi.station ==
                 WiFiStationState::ObtainingAddress &&
             phaseElapsedMs_ >= config_.dhcpTimeoutMs) {
    scheduleRetry(WiFiFailure::DhcpTimeout);
  } else if (snapshot_.wifi.station == WiFiStationState::RetryWait &&
             phaseElapsedMs_ >= snapshot_.wifi.retryDelayMs) {
    beginConnect();
  }
}

}  // namespace cadenza::system
