#pragma once

#include <atomic>
#include <cstdint>

#include "cadenza/system/connectivity_ingress.h"
#include "cadenza/system/connectivity_service.h"
#include "cadenza/system/system_service_host.h"
#include "esp_err.h"

struct ble_gap_event;

namespace cadenza::idf {

struct Security2Material {
  const char* salt = nullptr;
  std::uint16_t saltLength = 0;
  const char* verifier = nullptr;
  std::uint16_t verifierLength = 0;

  bool valid() const noexcept {
    return salt && saltLength != 0 && verifier && verifierLength != 0;
  }
};

// Production firmware supplies this at the privileged manufacturing/storage
// boundary. The candidate's weak default intentionally returns false and does
// not embed a shared salt, verifier, password, or proof-of-possession.
extern "C" bool cadenza_load_security2_material(
    Security2Material* material) noexcept;

class IdfConnectivityAdapter final
    : public system::ProvisioningPlatformPort {
 public:
  explicit IdfConnectivityAdapter(
      system::SystemServiceHost& host) noexcept;

  esp_err_t start() noexcept;
  void pump() noexcept;

  bool resetStoredCredentials(std::uint32_t generation) noexcept override;
  bool startSecurity2Provisioning(
      std::uint32_t generation) noexcept override;
  bool stopProvisioning(std::uint32_t generation) noexcept override;

 private:
  static void espEvent(void* context, const char* base,
                       std::int32_t id, void* data) noexcept;
  static void nimbleHostTask(void* context) noexcept;
  static void nimbleReset(int reason) noexcept;
  static void nimbleSync() noexcept;
  static int gapEvent(ble_gap_event* event, void* context) noexcept;

  void execute(const system::ConnectivityAction& action) noexcept;
  void handleEspEvent(const char* base, std::int32_t id,
                      void* data) noexcept;
  BluetoothLeSnapshot bluetoothSnapshot() const noexcept;
  void publishBluetoothFromCallback() noexcept;
  void publishBluetoothFromService() noexcept;
  esp_err_t startApplicationBluetooth() noexcept;
  esp_err_t stopApplicationBluetooth() noexcept;
  bool startAdvertising() noexcept;
  bool startScanning() noexcept;
  void reportProvisioningFailure(std::uint32_t generation,
                                 ProvisioningFailure failure,
                                 bool fromCallback) noexcept;

  system::SystemServiceHost* host_ = nullptr;
  system::ConnectivityIngressMux<32, 32> ingress_{};
  std::atomic<std::uint32_t> wifiGeneration_{0};
  std::atomic<std::uint32_t> bluetoothGeneration_{0};
  std::atomic<std::uint32_t> provisioningGeneration_{0};
  std::atomic<std::uint8_t> connectionCount_{0};
  std::atomic<bool> bluetoothSynced_{false};
  std::atomic<bool> bluetoothRadioRequested_{false};
  std::atomic<bool> advertising_{false};
  std::atomic<bool> scanning_{false};
  std::atomic<bool> provisioningActive_{false};
  bool wifiInitialized_ = false;
  bool provisioningInitialized_ = false;
  bool nimbleInitialized_ = false;
};

}  // namespace cadenza::idf
