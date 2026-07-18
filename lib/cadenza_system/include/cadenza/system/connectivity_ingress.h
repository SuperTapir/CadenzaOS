#pragma once

#include <atomic>
#include <cstddef>

#include "cadenza/system/platform_event_mailbox.h"
#include "cadenza/system/system_service_host.h"

namespace cadenza::system {

template <std::size_t WiFiCapacity, std::size_t BluetoothCapacity>
class ConnectivityIngressMux {
 public:
  bool postWiFiFromCallback(const PlatformEvent& event) noexcept {
    if (wifi_.postFromProducer(event)) return true;
    wifiOverflow_.store(true, std::memory_order_release);
    return false;
  }

  bool postBluetoothFromCallback(const PlatformEvent& event) noexcept {
    if (bluetooth_.postFromProducer(event)) return true;
    bluetoothOverflow_.store(true, std::memory_order_release);
    return false;
  }

  std::size_t drainFrame(SystemServiceHost& host, std::size_t wifiBudget,
                         std::size_t bluetoothBudget) noexcept {
    if (wifiOverflow_.exchange(false, std::memory_order_acq_rel)) {
      host.connectivityService().markWiFiResyncNeeded();
    }
    if (bluetoothOverflow_.exchange(false, std::memory_order_acq_rel)) {
      host.connectivityService().markBluetoothLeResyncNeeded();
    }
    std::size_t drained = drain(wifi_, host, wifiBudget);
    drained += drain(bluetooth_, host, bluetoothBudget);
    return drained;
  }

  SpscMailboxDiagnostics wifiDiagnostics() const noexcept {
    return wifi_.diagnostics();
  }
  SpscMailboxDiagnostics bluetoothDiagnostics() const noexcept {
    return bluetooth_.diagnostics();
  }

 private:
  template <std::size_t Capacity>
  static std::size_t drain(SpscMailbox<PlatformEvent, Capacity>& mailbox,
                           SystemServiceHost& host,
                           std::size_t budget) noexcept {
    std::size_t count = 0;
    PlatformEvent event;
    while (count < budget && mailbox.tryPop(event)) {
      if (!host.postPlatformEvent(event)) break;
      ++count;
    }
    return count;
  }

  SpscMailbox<PlatformEvent, WiFiCapacity> wifi_{};
  SpscMailbox<PlatformEvent, BluetoothCapacity> bluetooth_{};
  std::atomic<bool> wifiOverflow_{false};
  std::atomic<bool> bluetoothOverflow_{false};
};

}  // namespace cadenza::system
