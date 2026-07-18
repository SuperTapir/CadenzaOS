#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <atomic>
#include <array>
#include <algorithm>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <string>

#include "cadenza/system/platform_event_mailbox.h"
#include "cadenza/system/connectivity_service.h"
#include "cadenza/system/connectivity_ingress.h"
#include "cadenza/system/system_service_host.h"

namespace {

using CallbackMailbox = cadenza::system::PlatformEventMailbox<2>;

struct GenericEvent {
  std::uint32_t sequence = 0;
};

class FixedJitter final : public cadenza::system::RetryJitterSource {
 public:
  std::uint32_t next(std::uint32_t upperExclusive) noexcept override {
    lastUpperExclusive = upperExclusive;
    return value < upperExclusive ? value : 0;
  }
  std::uint32_t value = 0;
  std::uint32_t lastUpperExclusive = 0;
};

class CanaryProvisioningPort final
    : public cadenza::system::ProvisioningPlatformPort {
 public:
  explicit CanaryProvisioningPort(const char* secret) noexcept {
    while (secret[size_] != '\0' && size_ + 1 < secret_.size()) {
      secret_[size_] = secret[size_];
      ++size_;
    }
  }
  ~CanaryProvisioningPort() override { destroyFixture(); }

  bool resetStoredCredentials(std::uint32_t) noexcept override {
    ++resetCalls;
    return true;
  }
  bool startSecurity2Provisioning(std::uint32_t) noexcept override {
    ++startCalls;
    return true;
  }
  bool stopProvisioning(std::uint32_t) noexcept override {
    ++stopCalls;
    return true;
  }
  bool contains(const char* canary) const {
    return std::string(secret_.data(), size_).find(canary) != std::string::npos;
  }
  void destroyFixture() noexcept {
    std::fill(secret_.begin(), secret_.end(), '\0');
    size_ = 0;
  }

  std::uint32_t resetCalls = 0;
  std::uint32_t startCalls = 0;
  std::uint32_t stopCalls = 0;

 private:
  std::array<char, 64> secret_{};
  std::size_t size_ = 0;
};

class CapturingDiagnosticSink final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    last = event;
    ++count;
  }
  cadenza::DiagnosticEvent last{};
  std::uint32_t count = 0;
};

bool wifiAssociatedCallback(CallbackMailbox& mailbox,
                            std::uint32_t generation) noexcept {
  return mailbox.postFromCallback(
      cadenza::system::PlatformEvent::wifiAssociated(generation));
}

bool bluetoothSdkCallback(CallbackMailbox& mailbox,
                          const cadenza::BluetoothLeSnapshot& snapshot) noexcept {
  return mailbox.postFromCallback(
      cadenza::system::PlatformEvent::bluetoothObserved(snapshot));
}

void ingestCallbacks(CallbackMailbox& mailbox,
                     cadenza::system::SystemServiceHost& host) noexcept {
  cadenza::system::PlatformEvent event;
  while (mailbox.tryPop(event)) {
    if (!host.postPlatformEvent(event)) break;
  }
}

template <typename T, typename = void>
struct HasPlatformMailbox : std::false_type {};

template <typename T>
struct HasPlatformMailbox<
    T, std::void_t<decltype(std::declval<T&>().platformMailbox)>>
    : std::true_type {};

}  // namespace

static_assert(!HasPlatformMailbox<cadenza::AppUpdateContext>::value);
static_assert(!HasPlatformMailbox<cadenza::AppRenderContext>::value);
static_assert(std::is_trivially_copyable_v<cadenza::ConnectivitySnapshot>);
static_assert(sizeof(cadenza::SystemSnapshot) <= 96);

TEST_CASE("Wi-Fi service waits for IP before reporting online") {
  using namespace cadenza::system;
  ConnectivityService service;
  service.setWiFiAvailable(true);
  service.requestNetworkOnline(true);
  ConnectivityAction action;
  REQUIRE(service.tryPopAction(action));
  CHECK(action.type == ConnectivityActionType::StartWiFiRadio);
  CHECK_FALSE(service.tryPopAction(action));

  service.onWiFiRadioReady();
  REQUIRE(service.tryPopAction(action));
  REQUIRE(action.type == ConnectivityActionType::ConnectWiFi);
  REQUIRE(action.generation != 0);
  service.onWiFiAssociated(action.generation);
  CHECK(service.snapshot().wifi.station ==
        cadenza::WiFiStationState::ObtainingAddress);
  CHECK_FALSE(service.snapshot().summary().networkOnline);
  service.onWiFiGotIp(action.generation);
  CHECK(service.snapshot().summary().networkOnline);
}

TEST_CASE("explicit disconnect stays idle while external loss retries") {
  using namespace cadenza::system;
  ConnectivityService service{{100, 100, 5, 20, 3, 0}};
  service.setWiFiAvailable(true);
  service.requestNetworkOnline(true);
  ConnectivityAction action;
  REQUIRE(service.tryPopAction(action));
  service.onWiFiRadioReady();
  REQUIRE(service.tryPopAction(action));
  const std::uint32_t firstGeneration = action.generation;
  service.onWiFiAssociated(firstGeneration);
  service.onWiFiGotIp(firstGeneration);

  service.onWiFiDisconnected(firstGeneration,
                             cadenza::WiFiFailure::AccessPointNotFound);
  CHECK(service.snapshot().wifi.station ==
        cadenza::WiFiStationState::RetryWait);
  CHECK(service.snapshot().wifi.retryDelayMs == 5);
  service.advance(5);
  REQUIRE(service.tryPopAction(action));
  CHECK(action.type == ConnectivityActionType::ConnectWiFi);
  CHECK(action.generation != firstGeneration);

  service.requestNetworkOnline(false);
  REQUIRE(service.tryPopAction(action));
  CHECK(action.type == ConnectivityActionType::DisconnectWiFi);
  service.onWiFiDisconnected(action.generation, cadenza::WiFiFailure::None,
                             true);
  REQUIRE(service.tryPopAction(action));
  CHECK(action.type == ConnectivityActionType::StopWiFiRadio);
  service.advance(1000);
  CHECK_FALSE(service.tryPopAction(action));
  CHECK(service.snapshot().wifi.desired ==
        cadenza::WiFiDesiredPolicy::IdleAllowed);
  CHECK(service.snapshot().wifi.station == cadenza::WiFiStationState::Idle);
}

TEST_CASE("Wi-Fi timeout backoff failure mapping and stale generations are deterministic") {
  using namespace cadenza::system;
  ConnectivityService service{{2, 3, 4, 8, 2, 0xFFFFFFFEU}};
  service.setWiFiAvailable(true);
  service.requestNetworkOnline(true);
  ConnectivityAction action;
  REQUIRE(service.tryPopAction(action));
  service.onWiFiRadioReady();
  REQUIRE(service.tryPopAction(action));
  CHECK(action.generation == 0xFFFFFFFFU);
  service.advance(2);
  CHECK(service.snapshot().wifi.failure ==
        cadenza::WiFiFailure::AssociationTimeout);
  CHECK(service.snapshot().wifi.retryDelayMs == 4);
  service.advance(4);
  REQUIRE(service.tryPopAction(action));
  CHECK(action.generation == 1);
  service.onWiFiGotIp(0xFFFFFFFFU);
  CHECK(service.diagnostics().staleEvents == 1);
  CHECK_FALSE(service.snapshot().summary().networkOnline);
  service.onWiFiDisconnected(action.generation,
                             cadenza::WiFiFailure::AuthenticationFailed);
  CHECK(service.snapshot().wifi.station ==
        cadenza::WiFiStationState::NeedsUserAction);
  CHECK(service.snapshot().wifi.needsUserAction());
}

TEST_CASE("BLE roles coexist without changing network or USB voice") {
  cadenza::system::ConnectivityService service;
  service.setBluetoothLeAvailable(true);
  service.requestBluetoothAdvertising(true);
  service.requestBluetoothScanning(true);
  cadenza::system::ConnectivityAction action;
  REQUIRE(service.tryPopAction(action));
  const std::uint32_t generation = action.generation;
  service.onBluetoothLeRadioReady(generation);
  REQUIRE(service.tryPopAction(action));
  CHECK(action.type == cadenza::system::ConnectivityActionType::StartBluetoothLeAdvertising);
  REQUIRE(service.tryPopAction(action));
  CHECK(action.type == cadenza::system::ConnectivityActionType::StartBluetoothLeScanning);
  cadenza::BluetoothLeSnapshot bluetooth;
  bluetooth.generation = generation;
  bluetooth.radio = cadenza::BluetoothLeRadioState::Ready;
  bluetooth.advertising = true;
  bluetooth.scanning = true;
  bluetooth.connectionCount = 1;
  service.observeBluetoothLe(bluetooth);
  CHECK(service.snapshot().bluetoothLe.advertising);
  CHECK(service.snapshot().bluetoothLe.scanning);
  CHECK(service.snapshot().summary().bluetoothPeerConnected);
  CHECK_FALSE(service.snapshot().summary().networkOnline);
}

TEST_CASE("retry jitter and BLE scan timeout are injected bounded and replayable") {
  using namespace cadenza::system;
  FixedJitter jitter;
  jitter.value = 3;
  ConnectivityServiceConfig config;
  config.associationTimeoutMs = 1;
  config.initialRetryDelayMs = 4;
  config.maximumRetryDelayMs = 6;
  config.maximumRetryJitterMs = 5;
  config.retryJitter = &jitter;
  config.bluetoothScanTimeoutMs = 7;
  ConnectivityService service{config};
  service.setWiFiAvailable(true);
  service.requestNetworkOnline(true);
  ConnectivityAction action;
  REQUIRE(service.tryPopAction(action));
  service.onWiFiRadioReady();
  REQUIRE(service.tryPopAction(action));
  service.advance(1);
  CHECK(service.snapshot().wifi.retryDelayMs == 6);
  CHECK(jitter.lastUpperExclusive == 6);

  service.setBluetoothLeAvailable(true);
  service.requestBluetoothScanning(true);
  REQUIRE(service.tryPopAction(action));
  const std::uint32_t generation = action.generation;
  service.onBluetoothLeRadioReady(generation);
  REQUIRE(service.tryPopAction(action));
  CHECK(action.type == ConnectivityActionType::StartBluetoothLeScanning);
  service.advance(7);
  REQUIRE(service.tryPopAction(action));
  CHECK(action.type == ConnectivityActionType::StopBluetoothLeScanning);
  REQUIRE(service.tryPopAction(action));
  CHECK(action.type == ConnectivityActionType::StopBluetoothLeRadio);
  CHECK(service.snapshot().bluetoothLe.failure ==
        cadenza::BluetoothLeFailure::Timeout);
}

TEST_CASE("Wi-Fi retry attempt limit terminates instead of looping forever") {
  using namespace cadenza::system;
  ConnectivityServiceConfig config;
  config.associationTimeoutMs = 1;
  config.initialRetryDelayMs = 1;
  config.maximumRetryDelayMs = 2;
  config.maximumAttempts = 2;
  ConnectivityService service{config};
  service.setWiFiAvailable(true);
  service.requestNetworkOnline(true);
  ConnectivityAction action;
  REQUIRE(service.tryPopAction(action));
  service.onWiFiRadioReady();
  REQUIRE(service.tryPopAction(action));
  for (int attempt = 0; attempt < 2; ++attempt) {
    service.advance(1);
    CHECK(service.snapshot().wifi.station ==
          cadenza::WiFiStationState::RetryWait);
    CHECK(service.snapshot().wifi.retryDelayMs <= 2);
    service.advance(service.snapshot().wifi.retryDelayMs);
    REQUIRE(service.tryPopAction(action));
    CHECK(action.type == ConnectivityActionType::ConnectWiFi);
  }
  service.advance(1);
  CHECK(service.snapshot().wifi.failure ==
        cadenza::WiFiFailure::RetryExhausted);
  CHECK(service.snapshot().wifi.station ==
        cadenza::WiFiStationState::NeedsUserAction);
  service.advance(1000);
  CHECK_FALSE(service.tryPopAction(action));
}

TEST_CASE("provisioning session is exclusive owner-bound and generation-safe") {
  using namespace cadenza::system;
  ConnectivityService service;
  constexpr cadenza::AppId kOwnerId{0x6100};
  constexpr cadenza::AppId kOtherId{0x6101};
  const auto owner = cadenza::ResourceOwner::app(kOwnerId);
  const auto other = cadenza::ResourceOwner::app(kOtherId);
  CHECK(service.startProvisioning({}, false) ==
        ProvisioningRequestResult::InvalidOwner);
  REQUIRE(service.startProvisioning(owner, false) ==
          ProvisioningRequestResult::Started);
  const std::uint32_t generation =
      service.snapshot().provisioning.generation;
  REQUIRE(generation != 0);
  CHECK(service.startProvisioning(other, false) ==
        ProvisioningRequestResult::Busy);
  CHECK(service.cancelProvisioning(other) ==
        ProvisioningRequestResult::Unauthorized);

  service.onProvisioningProgress(generation + 1,
                                 cadenza::ProvisioningState::Succeeded);
  CHECK(service.diagnostics().staleEvents == 1);
  CHECK(service.snapshot().provisioning.state ==
        cadenza::ProvisioningState::Advertising);
  CHECK(service.cancelProvisioning(owner) ==
        ProvisioningRequestResult::Cancelled);
  ConnectivityAction action;
  REQUIRE(service.tryPopAction(action));
  CHECK(action.type == ConnectivityActionType::StartProvisioning);
  REQUIRE(service.tryPopAction(action));
  CHECK(action.type == ConnectivityActionType::StopProvisioning);
  cadenza::ResourceOwner released;
  REQUIRE(service.takeReleasedProvisioningOwner(released));
  CHECK(released == owner);
  service.onProvisioningStopped(generation);
  CHECK(service.snapshot().provisioning.state ==
        cadenza::ProvisioningState::Failed);
  CHECK(service.snapshot().provisioning.failure ==
        cadenza::ProvisioningFailure::Cancelled);
}

TEST_CASE("provisioning timeout retry and confirmed reset use fresh generation") {
  using namespace cadenza::system;
  ConnectivityServiceConfig config;
  config.provisioningTimeoutMs = 5;
  ConnectivityService service{config};
  const auto owner = cadenza::ResourceOwner::system();
  REQUIRE(service.startProvisioning(owner, false) ==
          ProvisioningRequestResult::Started);
  const std::uint32_t first = service.snapshot().provisioning.generation;
  ConnectivityAction action;
  REQUIRE(service.tryPopAction(action));
  service.advance(5);
  CHECK(service.snapshot().provisioning.state ==
        cadenza::ProvisioningState::Stopping);
  CHECK(service.snapshot().provisioning.failure ==
        cadenza::ProvisioningFailure::Timeout);
  REQUIRE(service.tryPopAction(action));
  CHECK(action.type == ConnectivityActionType::StopProvisioning);
  service.onProvisioningStopped(first);
  CHECK(service.snapshot().provisioning.state ==
        cadenza::ProvisioningState::Failed);

  REQUIRE(service.startProvisioning(owner, true) ==
          ProvisioningRequestResult::Started);
  CHECK(service.snapshot().provisioning.generation != first);
  REQUIRE(service.tryPopAction(action));
  CHECK(action.type == ConnectivityActionType::ResetCredentials);
  REQUIRE(service.tryPopAction(action));
  CHECK(action.type == ConnectivityActionType::StartProvisioning);
}

TEST_CASE("portable provisioning artifacts cannot carry credential canary") {
  constexpr const char* kSecretCanary =
      "CADENZA_SECRET_CANARY_7f9e2a_do_not_log";
  CanaryProvisioningPort privilegedPort{kSecretCanary};
  REQUIRE(privilegedPort.contains(kSecretCanary));
  cadenza::system::ConnectivityService service;
  REQUIRE(service.startProvisioning(cadenza::ResourceOwner::system(), true) ==
          cadenza::system::ProvisioningRequestResult::Started);
  cadenza::system::ConnectivityAction action;
  REQUIRE(service.tryPopAction(action));
  REQUIRE(action.type ==
          cadenza::system::ConnectivityActionType::ResetCredentials);
  REQUIRE(privilegedPort.resetStoredCredentials(action.generation));
  REQUIRE(service.tryPopAction(action));
  REQUIRE(action.type ==
          cadenza::system::ConnectivityActionType::StartProvisioning);
  REQUIRE(privilegedPort.startSecurity2Provisioning(action.generation));
  service.onProvisioningProgress(
      service.snapshot().provisioning.generation,
      cadenza::ProvisioningState::Failed,
      cadenza::ProvisioningFailure::AuthenticationFailed);
  REQUIRE(service.tryPopAction(action));
  REQUIRE(action.type ==
          cadenza::system::ConnectivityActionType::StopProvisioning);
  REQUIRE(privilegedPort.stopProvisioning(action.generation));
  service.onProvisioningStopped(action.generation);

  REQUIRE(service.startProvisioning(cadenza::ResourceOwner::system()) ==
          cadenza::system::ProvisioningRequestResult::Started);
  REQUIRE(service.tryPopAction(action));
  REQUIRE(action.type ==
          cadenza::system::ConnectivityActionType::StartProvisioning);
  REQUIRE(privilegedPort.startSecurity2Provisioning(action.generation));
  service.onProvisioningProgress(action.generation,
                                 cadenza::ProvisioningState::Succeeded);
  REQUIRE(service.tryPopAction(action));
  REQUIRE(action.type ==
          cadenza::system::ConnectivityActionType::StopProvisioning);
  REQUIRE(privilegedPort.stopProvisioning(action.generation));
  service.onProvisioningStopped(action.generation);

  CapturingDiagnosticSink sink;
  cadenza::system::SystemServiceHost host;
  host.setDiagnosticSink(&sink);
  REQUIRE(host.submit(cadenza::SystemCommand::emitDiagnostic(
      {cadenza::DiagnosticCategory::Runtime,
       cadenza::DiagnosticCode::InvalidOperation, kSecretCanary, 73})));
  host.commitCommands();
  REQUIRE(sink.count == 1);
  REQUIRE(sink.last.message != nullptr);

  cadenza::system::SystemServiceHost overflowHost{{1, 1, 1, 1}};
  REQUIRE(overflowHost.postPlatformEvent(
      cadenza::system::PlatformEvent::wifiAvailability(true)));
  CHECK_FALSE(overflowHost.postPlatformEvent(
      cadenza::system::PlatformEvent::wifiAvailability(false)));

  const auto& snapshot = service.snapshot();
  const auto& diagnostics = service.diagnostics();
  const std::string ordinaryDump =
      std::to_string(static_cast<unsigned>(snapshot.provisioning.state)) + ":" +
      std::to_string(static_cast<unsigned>(snapshot.provisioning.failure)) + ":" +
      std::to_string(snapshot.provisioning.generation) + ":" +
      std::to_string(diagnostics.staleEvents) + ":" + sink.last.message + ":" +
      std::to_string(sink.last.value) + ":" +
      std::to_string(overflowHost.diagnostics().rejectedPlatformEvents);
  CHECK(ordinaryDump.find(kSecretCanary) == std::string::npos);
  CHECK(std::string(sink.last.message).find(kSecretCanary) ==
        std::string::npos);
  CHECK(sink.last.value == 73);
  CHECK(privilegedPort.resetCalls == 1);
  CHECK(privilegedPort.startCalls == 2);
  CHECK(privilegedPort.stopCalls == 2);
  CHECK(std::is_trivially_copyable_v<cadenza::ProvisioningSnapshot>);
  CHECK(std::is_trivially_copyable_v<cadenza::system::ConnectivityAction>);
  privilegedPort.destroyFixture();
  CHECK_FALSE(privilegedPort.contains(kSecretCanary));
}

TEST_CASE("Wi-Fi association IP readiness and BLE roles remain independent") {
  cadenza::ConnectivitySnapshot snapshot;
  snapshot.wifi.radio = cadenza::WiFiRadioState::Ready;
  snapshot.wifi.station = cadenza::WiFiStationState::ObtainingAddress;
  snapshot.bluetoothLe.radio = cadenza::BluetoothLeRadioState::Ready;
  snapshot.bluetoothLe.advertising = true;
  snapshot.bluetoothLe.connectionCount = 1;
  CHECK_FALSE(snapshot.summary().networkOnline);
  CHECK(snapshot.summary().bluetoothReady);
  CHECK(snapshot.summary().bluetoothPeerConnected);
  CHECK(snapshot.bluetoothLe.advertising);

  snapshot.wifi.station = cadenza::WiFiStationState::Online;
  CHECK(snapshot.summary().networkOnline);
  CHECK(snapshot.bluetoothLe.connectionCount == 1);
}

TEST_CASE("connectivity callbacks cross two bounded ingestion stages") {
  CallbackMailbox mailbox;
  cadenza::system::SystemServiceHost host;
  host.connectivityService().setWiFiAvailable(true);
  host.connectivityService().requestNetworkOnline(true);
  cadenza::system::ConnectivityAction driverAction;
  REQUIRE(host.connectivityService().tryPopAction(driverAction));
  host.connectivityService().onWiFiRadioReady();
  REQUIRE(host.connectivityService().tryPopAction(driverAction));
  const std::uint32_t generation = driverAction.generation;
  host.connectivityService().setBluetoothLeAvailable(true);
  host.connectivityService().requestBluetoothAdvertising(true);
  REQUIRE(host.connectivityService().tryPopAction(driverAction));
  const std::uint32_t bluetoothGeneration = driverAction.generation;
  host.connectivityService().onBluetoothLeRadioReady(bluetoothGeneration);
  REQUIRE(host.connectivityService().tryPopAction(driverAction));

  cadenza::BluetoothLeSnapshot bluetooth;
  bluetooth.radio = cadenza::BluetoothLeRadioState::Ready;
  bluetooth.generation = bluetoothGeneration;
  bluetooth.advertising = true;
  bluetooth.connectionCount = 1;
  REQUIRE(wifiAssociatedCallback(mailbox, generation));
  REQUIRE(bluetoothSdkCallback(mailbox, bluetooth));
  CHECK(host.snapshot().connectivity.wifi.radio ==
        cadenza::WiFiRadioState::Unavailable);
  CHECK(host.snapshot().connectivity.bluetoothLe.radio ==
        cadenza::BluetoothLeRadioState::Unavailable);

  ingestCallbacks(mailbox, host);
  CHECK(host.snapshot().connectivity.wifi.radio ==
        cadenza::WiFiRadioState::Unavailable);
  CHECK(host.pendingPlatformEventCount() == 2);

  const auto& snapshot = host.beginFrame(0.0F);
  CHECK(snapshot.connectivity.wifi.station ==
        cadenza::WiFiStationState::ObtainingAddress);
  CHECK_FALSE(snapshot.connectivity.summary().networkOnline);
  CHECK(snapshot.connectivity.bluetoothLe.connectionCount == 1);
  REQUIRE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::wifiGotIp(generation)));
  CHECK(host.beginFrame(0.0F).connectivity.summary().networkOnline);
  CHECK(mailbox.diagnostics().posted == 2);
  CHECK(mailbox.diagnostics().consumed == 2);
  CHECK(mailbox.diagnostics().highWater == 2);
}

TEST_CASE("callback mailbox rejects overflow without overwriting events") {
  CallbackMailbox mailbox;
  REQUIRE(wifiAssociatedCallback(mailbox, 10));
  REQUIRE(wifiAssociatedCallback(mailbox, 11));
  CHECK_FALSE(wifiAssociatedCallback(mailbox, 12));
  CHECK(mailbox.diagnostics().rejected == 1);

  cadenza::system::PlatformEvent first;
  cadenza::system::PlatformEvent second;
  REQUIRE(mailbox.tryPop(first));
  REQUIRE(mailbox.tryPop(second));
  CHECK(first.generation == 10);
  CHECK(second.generation == 11);
  CHECK_FALSE(mailbox.tryPop(first));
}

TEST_CASE("independent ingress budgets merge deterministically and overflow resyncs") {
  cadenza::system::ConnectivityIngressMux<2, 2> ingress;
  cadenza::system::SystemServiceHost host;
  REQUIRE(ingress.postWiFiFromCallback(
      cadenza::system::PlatformEvent::wifiAssociated(1)));
  REQUIRE(ingress.postWiFiFromCallback(
      cadenza::system::PlatformEvent::wifiAssociated(2)));
  CHECK_FALSE(ingress.postWiFiFromCallback(
      cadenza::system::PlatformEvent::wifiGotIp(2)));
  cadenza::BluetoothLeSnapshot bluetooth;
  bluetooth.generation = 1;
  bluetooth.radio = cadenza::BluetoothLeRadioState::Ready;
  REQUIRE(ingress.postBluetoothFromCallback(
      cadenza::system::PlatformEvent::bluetoothObserved(bluetooth)));
  bluetooth.connectionCount = 1;
  REQUIRE(ingress.postBluetoothFromCallback(
      cadenza::system::PlatformEvent::bluetoothObserved(bluetooth)));
  CHECK_FALSE(ingress.postBluetoothFromCallback(
      cadenza::system::PlatformEvent::bluetoothObserved(bluetooth)));

  CHECK(ingress.drainFrame(host, 1, 1) == 2);
  CHECK(host.pendingPlatformEventCount() == 2);
  CHECK(ingress.wifiDiagnostics().consumed == 1);
  CHECK(ingress.bluetoothDiagnostics().consumed == 1);
  const auto& overflowed = host.beginFrame(0.0F);
  CHECK(overflowed.connectivity.wifi.resyncNeeded);
  CHECK(overflowed.connectivity.bluetoothLe.resyncNeeded);

  cadenza::WiFiSnapshot authoritativeWifi;
  authoritativeWifi.radio = cadenza::WiFiRadioState::Ready;
  authoritativeWifi.station = cadenza::WiFiStationState::Online;
  authoritativeWifi.generation = 9;
  host.connectivityService().reconcileWiFi(authoritativeWifi);
  cadenza::BluetoothLeSnapshot authoritativeBluetooth;
  authoritativeBluetooth.radio = cadenza::BluetoothLeRadioState::Ready;
  authoritativeBluetooth.generation = 7;
  authoritativeBluetooth.connectionCount = 1;
  host.connectivityService().reconcileBluetoothLe(authoritativeBluetooth);
  CHECK_FALSE(host.connectivityService().snapshot().wifi.resyncNeeded);
  CHECK_FALSE(host.connectivityService().snapshot().bluetoothLe.resyncNeeded);
  CHECK(host.connectivityService().snapshot().summary().networkOnline);
  CHECK(host.connectivityService().snapshot().summary().bluetoothPeerConnected);

  host.connectivityService().markWiFiResyncNeeded();
  host.connectivityService().failWiFiReconcile();
  CHECK(host.connectivityService().snapshot().wifi.radio ==
        cadenza::WiFiRadioState::Degraded);
  CHECK_FALSE(host.connectivityService().snapshot().wifi.resyncNeeded);
}

TEST_CASE("generic SPSC mailbox preserves FIFO and rejects newest when full") {
  cadenza::system::SpscMailbox<GenericEvent, 2> mailbox;
  REQUIRE(mailbox.postFromProducer({10}));
  REQUIRE(mailbox.postFromProducer({11}));
  CHECK_FALSE(mailbox.postFromProducer({12}));

  GenericEvent event;
  REQUIRE(mailbox.tryPop(event));
  CHECK(event.sequence == 10);
  REQUIRE(mailbox.tryPop(event));
  CHECK(event.sequence == 11);
  CHECK_FALSE(mailbox.tryPop(event));
  CHECK(mailbox.diagnostics().posted == 2);
  CHECK(mailbox.diagnostics().consumed == 2);
  CHECK(mailbox.diagnostics().rejected == 1);
  CHECK(mailbox.diagnostics().highWater == 2);
}

TEST_CASE("generic SPSC mailbox crosses modular index wraparound") {
  cadenza::system::SpscMailbox<GenericEvent, 4, std::uint8_t> mailbox;
  for (std::uint32_t sequence = 0; sequence < 600; ++sequence) {
    REQUIRE(mailbox.postFromProducer({sequence}));
    GenericEvent event;
    REQUIRE(mailbox.tryPop(event));
    REQUIRE(event.sequence == sequence);
  }
  CHECK(mailbox.diagnostics().posted == 600);
  CHECK(mailbox.diagnostics().consumed == 600);
  CHECK(mailbox.diagnostics().rejected == 0);
  CHECK(mailbox.diagnostics().highWater == 1);
}

TEST_CASE("generic SPSC mailbox transfers values between real threads") {
  constexpr std::uint32_t kEventCount = 20000;
  cadenza::system::SpscMailbox<GenericEvent, 32> mailbox;
  std::atomic<bool> producerDone{false};
  std::atomic<bool> sequenceFailed{false};

  std::thread producer([&] {
    for (std::uint32_t sequence = 0; sequence < kEventCount; ++sequence) {
      while (!mailbox.postFromProducer({sequence})) std::this_thread::yield();
    }
    producerDone.store(true, std::memory_order_release);
  });
  std::thread consumer([&] {
    std::uint32_t expected = 0;
    while (expected < kEventCount) {
      GenericEvent event;
      if (!mailbox.tryPop(event)) {
        if (producerDone.load(std::memory_order_acquire) &&
            mailbox.diagnostics().consumed == kEventCount) {
          break;
        }
        std::this_thread::yield();
        continue;
      }
      if (event.sequence != expected) sequenceFailed.store(true);
      ++expected;
    }
  });
  producer.join();
  consumer.join();

  CHECK_FALSE(sequenceFailed.load());
  CHECK(mailbox.diagnostics().posted == kEventCount);
  CHECK(mailbox.diagnostics().consumed == kEventCount);
  CHECK(mailbox.diagnostics().highWater <= 32);
}
