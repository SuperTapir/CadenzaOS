#include "idf_connectivity_adapter.h"

#include <cstring>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/ble_hs_id.h"
#include "host/ble_hs_adv.h"
#include "host/util/util.h"
#include "nimble/ble.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "nvs_flash.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "wifi_provisioning/manager.h"
#include "wifi_provisioning/scheme_ble.h"

namespace cadenza::idf {

namespace {
IdfConnectivityAdapter* activeAdapter = nullptr;

WiFiFailure mapDisconnectReason(std::uint8_t reason) noexcept {
  switch (reason) {
    case WIFI_REASON_AUTH_EXPIRE:
    case WIFI_REASON_AUTH_FAIL:
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
      return WiFiFailure::AuthenticationFailed;
    case WIFI_REASON_NO_AP_FOUND:
      return WiFiFailure::AccessPointNotFound;
    default:
      return WiFiFailure::Driver;
  }
}
}  // namespace

extern "C" __attribute__((weak)) bool cadenza_load_security2_material(
    Security2Material* material) noexcept {
  if (material) *material = {};
  return false;
}

IdfConnectivityAdapter::IdfConnectivityAdapter(
    system::SystemServiceHost& host) noexcept
    : host_(&host) {}

esp_err_t IdfConnectivityAdapter::start() noexcept {
  esp_err_t result = nvs_flash_init();
  if (result == ESP_ERR_NVS_NO_FREE_PAGES ||
      result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    result = nvs_flash_erase();
    if (result == ESP_OK) result = nvs_flash_init();
  }
  if (result != ESP_OK) return result;
  result = esp_netif_init();
  if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return result;
  result = esp_event_loop_create_default();
  if (result != ESP_OK && result != ESP_ERR_INVALID_STATE) return result;
  if (!esp_netif_create_default_wifi_sta()) return ESP_ERR_NO_MEM;

  wifi_init_config_t wifiConfig = WIFI_INIT_CONFIG_DEFAULT();
  result = esp_wifi_init(&wifiConfig);
  if (result != ESP_OK) return result;
  wifiInitialized_ = true;
  result = esp_wifi_set_mode(WIFI_MODE_STA);
  if (result != ESP_OK) return result;
  result = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                      &IdfConnectivityAdapter::espEvent, this);
  if (result != ESP_OK) return result;
  result = esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                      &IdfConnectivityAdapter::espEvent, this);
  if (result != ESP_OK) return result;
  result = esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID,
                                      &IdfConnectivityAdapter::espEvent, this);
  if (result != ESP_OK) return result;

  wifi_prov_mgr_config_t provisioningConfig{};
  provisioningConfig.scheme = wifi_prov_scheme_ble;
  provisioningConfig.scheme_event_handler = WIFI_PROV_EVENT_HANDLER_NONE;
  result = wifi_prov_mgr_init(provisioningConfig);
  if (result != ESP_OK) return result;
  provisioningInitialized_ = true;

  activeAdapter = this;

  host_->postPlatformEvent(system::PlatformEvent::wifiAvailability(true));
  host_->postPlatformEvent(
      system::PlatformEvent::bluetoothAvailability(true));
  return ESP_OK;
}

void IdfConnectivityAdapter::pump() noexcept {
  ingress_.drainFrame(*host_, 8, 8);
  system::ConnectivityAction action;
  while (host_->connectivityService().tryPopAction(action)) execute(action);
}

void IdfConnectivityAdapter::espEvent(void* context, const char* base,
                                      std::int32_t id, void* data) noexcept {
  static_cast<IdfConnectivityAdapter*>(context)->handleEspEvent(base, id, data);
}

void IdfConnectivityAdapter::handleEspEvent(
    const char* base, std::int32_t id, void* data) noexcept {
  if (base == WIFI_EVENT) {
    if (id == WIFI_EVENT_STA_START) {
      ingress_.postWiFiFromCallback(
          system::PlatformEvent::wifiRadioReady());
    } else if (id == WIFI_EVENT_STA_STOP) {
      ingress_.postWiFiFromCallback(
          system::PlatformEvent::wifiRadioStopped());
    } else if (id == WIFI_EVENT_STA_CONNECTED) {
      ingress_.postWiFiFromCallback(system::PlatformEvent::wifiAssociated(
          wifiGeneration_.load(std::memory_order_acquire)));
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
      const auto* disconnected =
          static_cast<const wifi_event_sta_disconnected_t*>(data);
      ingress_.postWiFiFromCallback(system::PlatformEvent::wifiDisconnected(
          wifiGeneration_.load(std::memory_order_acquire),
          disconnected ? mapDisconnectReason(disconnected->reason)
                       : WiFiFailure::Driver));
    }
    return;
  }
  if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
    ingress_.postWiFiFromCallback(system::PlatformEvent::wifiGotIp(
        wifiGeneration_.load(std::memory_order_acquire)));
    return;
  }
  if (base != WIFI_PROV_EVENT) return;
  const std::uint32_t generation =
      provisioningGeneration_.load(std::memory_order_acquire);
  switch (id) {
    case WIFI_PROV_CRED_RECV:
      ingress_.postWiFiFromCallback(
          system::PlatformEvent::provisioningProgress(
              generation, ProvisioningState::Negotiating));
      break;
    case WIFI_PROV_CRED_SUCCESS:
      ingress_.postWiFiFromCallback(
          system::PlatformEvent::provisioningProgress(
              generation, ProvisioningState::Succeeded));
      break;
    case WIFI_PROV_CRED_FAIL: {
      const auto reason = data ? *static_cast<wifi_prov_sta_fail_reason_t*>(data)
                               : WIFI_PROV_STA_AUTH_ERROR;
      reportProvisioningFailure(
          generation,
          reason == WIFI_PROV_STA_AP_NOT_FOUND
              ? ProvisioningFailure::AccessPointNotFound
              : ProvisioningFailure::AuthenticationFailed,
          true);
      break;
    }
    case WIFI_PROV_END:
      provisioningActive_.store(false, std::memory_order_release);
      ingress_.postWiFiFromCallback(
          system::PlatformEvent::provisioningStopped(generation));
      break;
    default:
      break;
  }
}

void IdfConnectivityAdapter::nimbleHostTask(void*) noexcept {
  nimble_port_run();
  nimble_port_freertos_deinit();
}

void IdfConnectivityAdapter::nimbleReset(int) noexcept {
  if (!activeAdapter) return;
  activeAdapter->bluetoothSynced_.store(false, std::memory_order_release);
  BluetoothLeSnapshot snapshot;
  snapshot.radio = BluetoothLeRadioState::Degraded;
  snapshot.failure = BluetoothLeFailure::Driver;
  snapshot.generation = activeAdapter->bluetoothGeneration_.load(
      std::memory_order_acquire);
  activeAdapter->ingress_.postBluetoothFromCallback(
      system::PlatformEvent::bluetoothObserved(snapshot));
}

void IdfConnectivityAdapter::nimbleSync() noexcept {
  if (!activeAdapter) return;
  activeAdapter->bluetoothSynced_.store(true, std::memory_order_release);
  if (activeAdapter->bluetoothRadioRequested_.load(
          std::memory_order_acquire)) {
    activeAdapter->ingress_.postBluetoothFromCallback(
        system::PlatformEvent::bluetoothRadioReady(
            activeAdapter->bluetoothGeneration_.load(
                std::memory_order_acquire)));
  }
}

int IdfConnectivityAdapter::gapEvent(ble_gap_event* event,
                                     void* context) noexcept {
  auto* adapter = static_cast<IdfConnectivityAdapter*>(context);
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      if (event->connect.status == 0) {
        adapter->connectionCount_.fetch_add(1, std::memory_order_acq_rel);
      } else {
        adapter->advertising_.store(false, std::memory_order_release);
      }
      adapter->publishBluetoothFromCallback();
      break;
    case BLE_GAP_EVENT_DISCONNECT: {
      std::uint8_t count = adapter->connectionCount_.load(
          std::memory_order_acquire);
      if (count != 0) {
        adapter->connectionCount_.store(count - 1, std::memory_order_release);
      }
      adapter->publishBluetoothFromCallback();
      break;
    }
    case BLE_GAP_EVENT_ADV_COMPLETE:
      adapter->advertising_.store(false, std::memory_order_release);
      adapter->publishBluetoothFromCallback();
      break;
    case BLE_GAP_EVENT_DISC_COMPLETE:
      adapter->scanning_.store(false, std::memory_order_release);
      adapter->publishBluetoothFromCallback();
      break;
    default:
      break;
  }
  return 0;
}

BluetoothLeSnapshot IdfConnectivityAdapter::bluetoothSnapshot() const noexcept {
  BluetoothLeSnapshot snapshot;
  snapshot.radio = bluetoothSynced_.load(std::memory_order_acquire)
                       ? BluetoothLeRadioState::Ready
                       : BluetoothLeRadioState::Starting;
  snapshot.generation =
      bluetoothGeneration_.load(std::memory_order_acquire);
  snapshot.advertising = advertising_.load(std::memory_order_acquire);
  snapshot.scanning = scanning_.load(std::memory_order_acquire);
  snapshot.connectionCount = connectionCount_.load(std::memory_order_acquire);
  return snapshot;
}

void IdfConnectivityAdapter::publishBluetoothFromCallback() noexcept {
  ingress_.postBluetoothFromCallback(
      system::PlatformEvent::bluetoothObserved(bluetoothSnapshot()));
}

void IdfConnectivityAdapter::publishBluetoothFromService() noexcept {
  host_->postPlatformEvent(
      system::PlatformEvent::bluetoothObserved(bluetoothSnapshot()));
}

esp_err_t IdfConnectivityAdapter::startApplicationBluetooth() noexcept {
  if (nimbleInitialized_) return ESP_OK;
  if (provisioningActive_.load(std::memory_order_acquire)) {
    return ESP_ERR_INVALID_STATE;
  }
  esp_err_t result = nimble_port_init();
  if (result != ESP_OK) return result;
  ble_svc_gap_init();
  ble_svc_gatt_init();
  ble_svc_gap_device_name_set("CadenzaOS");
  ble_hs_cfg.reset_cb = &IdfConnectivityAdapter::nimbleReset;
  ble_hs_cfg.sync_cb = &IdfConnectivityAdapter::nimbleSync;
  nimbleInitialized_ = true;
  nimble_port_freertos_init(&IdfConnectivityAdapter::nimbleHostTask);
  return ESP_OK;
}

esp_err_t IdfConnectivityAdapter::stopApplicationBluetooth() noexcept {
  if (!nimbleInitialized_) return ESP_OK;
  ble_gap_adv_stop();
  ble_gap_disc_cancel();
  const int result = nimble_port_stop();
  if (result != 0) return ESP_FAIL;
  nimble_port_deinit();
  nimbleInitialized_ = false;
  bluetoothSynced_.store(false, std::memory_order_release);
  advertising_.store(false, std::memory_order_release);
  scanning_.store(false, std::memory_order_release);
  connectionCount_.store(0, std::memory_order_release);
  return ESP_OK;
}

bool IdfConnectivityAdapter::startAdvertising() noexcept {
  std::uint8_t addressType = 0;
  if (ble_hs_util_ensure_addr(0) != 0 ||
      ble_hs_id_infer_auto(0, &addressType) != 0) {
    return false;
  }
  ble_hs_adv_fields fields{};
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.name = reinterpret_cast<const std::uint8_t*>("CadenzaOS");
  fields.name_len = 9;
  fields.name_is_complete = 1;
  if (ble_gap_adv_set_fields(&fields) != 0) return false;
  ble_gap_adv_params parameters{};
  parameters.conn_mode = BLE_GAP_CONN_MODE_UND;
  parameters.disc_mode = BLE_GAP_DISC_MODE_GEN;
  if (ble_gap_adv_start(addressType, nullptr, BLE_HS_FOREVER, &parameters,
                        &gapEvent, this) != 0) {
    return false;
  }
  advertising_.store(true, std::memory_order_release);
  publishBluetoothFromService();
  return true;
}

bool IdfConnectivityAdapter::startScanning() noexcept {
  std::uint8_t addressType = 0;
  if (ble_hs_id_infer_auto(0, &addressType) != 0) return false;
  ble_gap_disc_params parameters{};
  parameters.passive = 1;
  parameters.filter_duplicates = 1;
  if (ble_gap_disc(addressType, BLE_HS_FOREVER, &parameters,
                   &gapEvent, this) != 0) {
    return false;
  }
  scanning_.store(true, std::memory_order_release);
  publishBluetoothFromService();
  return true;
}

void IdfConnectivityAdapter::reportProvisioningFailure(
    std::uint32_t generation, ProvisioningFailure failure,
    bool fromCallback) noexcept {
  const auto event = system::PlatformEvent::provisioningProgress(
      generation, ProvisioningState::Failed, failure);
  if (fromCallback) {
    ingress_.postWiFiFromCallback(event);
  } else {
    host_->postPlatformEvent(event);
  }
}

bool IdfConnectivityAdapter::resetStoredCredentials(
    std::uint32_t) noexcept {
  return provisioningInitialized_ &&
         wifi_prov_mgr_reset_provisioning() == ESP_OK;
}

bool IdfConnectivityAdapter::startSecurity2Provisioning(
    std::uint32_t generation) noexcept {
  Security2Material material;
  if (bluetoothRadioRequested_.load(std::memory_order_acquire) ||
      nimbleInitialized_ ||
      provisioningActive_.load(std::memory_order_acquire)) {
    reportProvisioningFailure(generation, ProvisioningFailure::Busy, false);
    return false;
  }
  if (!provisioningInitialized_ ||
      !cadenza_load_security2_material(&material) || !material.valid()) {
    reportProvisioningFailure(generation,
                              ProvisioningFailure::Transport, false);
    return false;
  }
  provisioningGeneration_.store(generation, std::memory_order_release);
  wifi_prov_security2_params_t parameters{};
  parameters.salt = material.salt;
  parameters.salt_len = material.saltLength;
  parameters.verifier = material.verifier;
  parameters.verifier_len = material.verifierLength;
  const esp_err_t result = wifi_prov_mgr_start_provisioning(
      WIFI_PROV_SECURITY_2, &parameters, "CADENZA-SETUP", nullptr);
  if (result != ESP_OK) {
    reportProvisioningFailure(generation,
                              ProvisioningFailure::Transport, false);
  } else {
    provisioningActive_.store(true, std::memory_order_release);
  }
  return result == ESP_OK;
}

bool IdfConnectivityAdapter::stopProvisioning(
    std::uint32_t) noexcept {
  if (!provisioningInitialized_) return false;
  wifi_prov_mgr_stop_provisioning();
  return true;
}

void IdfConnectivityAdapter::execute(
    const system::ConnectivityAction& action) noexcept {
  using system::ConnectivityActionType;
  esp_err_t result = ESP_OK;
  switch (action.type) {
    case ConnectivityActionType::StartWiFiRadio:
      result = wifiInitialized_ ? esp_wifi_start() : ESP_ERR_INVALID_STATE;
      break;
    case ConnectivityActionType::StopWiFiRadio:
      result = wifiInitialized_ ? esp_wifi_stop() : ESP_ERR_INVALID_STATE;
      break;
    case ConnectivityActionType::ConnectWiFi:
      wifiGeneration_.store(action.generation, std::memory_order_release);
      result = esp_wifi_connect();
      break;
    case ConnectivityActionType::DisconnectWiFi:
      wifiGeneration_.store(action.generation, std::memory_order_release);
      result = esp_wifi_disconnect();
      break;
    case ConnectivityActionType::ResetCredentials:
      if (!resetStoredCredentials(action.generation)) result = ESP_FAIL;
      break;
    case ConnectivityActionType::StartProvisioning:
      if (!startSecurity2Provisioning(action.generation)) result = ESP_FAIL;
      break;
    case ConnectivityActionType::StopProvisioning:
      stopProvisioning(action.generation);
      return;
    case ConnectivityActionType::StartBluetoothLeRadio:
      bluetoothGeneration_.store(action.generation,
                                 std::memory_order_release);
      bluetoothRadioRequested_.store(true, std::memory_order_release);
      if (startApplicationBluetooth() != ESP_OK) {
        bluetoothRadioRequested_.store(false, std::memory_order_release);
        BluetoothLeSnapshot snapshot;
        snapshot.radio = BluetoothLeRadioState::Degraded;
        snapshot.failure = BluetoothLeFailure::CapacityExceeded;
        snapshot.generation = action.generation;
        host_->postPlatformEvent(
            system::PlatformEvent::bluetoothObserved(snapshot));
        return;
      }
      if (bluetoothSynced_.load(std::memory_order_acquire)) {
        host_->postPlatformEvent(
            system::PlatformEvent::bluetoothRadioReady(action.generation));
      }
      return;
    case ConnectivityActionType::StopBluetoothLeRadio:
      bluetoothRadioRequested_.store(false, std::memory_order_release);
      stopApplicationBluetooth();
      host_->postPlatformEvent(
          system::PlatformEvent::bluetoothRadioStopped(action.generation));
      return;
    case ConnectivityActionType::StartBluetoothLeAdvertising:
      if (!startAdvertising()) publishBluetoothFromService();
      return;
    case ConnectivityActionType::StopBluetoothLeAdvertising:
      ble_gap_adv_stop();
      advertising_.store(false, std::memory_order_release);
      publishBluetoothFromService();
      return;
    case ConnectivityActionType::StartBluetoothLeScanning:
      if (!startScanning()) publishBluetoothFromService();
      return;
    case ConnectivityActionType::StopBluetoothLeScanning:
      ble_gap_disc_cancel();
      scanning_.store(false, std::memory_order_release);
      publishBluetoothFromService();
      return;
  }
  if (result != ESP_OK && action.generation != 0) {
    host_->connectivityService().onWiFiDriverFailure(
        action.generation, WiFiFailure::Driver);
  }
}

}  // namespace cadenza::idf
