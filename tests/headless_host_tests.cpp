#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

#include "cadenza/host/headless_host.h"

namespace {
bool traceContains(
    const cadenza::host::HeadlessConnectivityAdapter& adapter,
    cadenza::system::ConnectivityActionType type) noexcept {
  for (std::size_t index = 0; index < adapter.traceSize(); ++index) {
    const auto* action = adapter.traceAt(index);
    if (action && action->type == type) return true;
  }
  return false;
}

class TickApp final : public cadenza::App {
 public:
  const char* name() const noexcept override { return "Tick"; }
  void update(const cadenza::AppUpdateContext& context) noexcept override {
    elapsed += context.dt;
    turn += context.input.turn;
    ++updates;
  }
  void render(cadenza::MonoCanvas& canvas,
              const cadenza::AppRenderContext&) noexcept override {
    canvas.clear(false);
    canvas.pixel(updates, turn + 10);
  }

  float elapsed = 0.0F;
  int turn = 0;
  int updates = 0;
};
}  // namespace

TEST_CASE("deterministic runner advances only by its fixed delta") {
  TickApp app;
  cadenza::AppRuntime runtime;
  REQUIRE(runtime.registerApp(cadenza::apps::kLauncherAppId, app, false));
  REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  cadenza::host::DeterministicRunner runner{runtime, canvas, framebuffer,
                                             1.0F / 60.0F};

  runner.step();
  runner.step();
  runner.step();
  CHECK(runner.frameIndex() == 3);
  CHECK(std::abs(runner.simulationSeconds() - 0.05F) < 0.00001F);
  CHECK(std::abs(app.elapsed - 0.05F) < 0.00001F);
}

TEST_CASE("scripted input is delivered on exact simulation frames") {
  TickApp app;
  cadenza::AppRuntime runtime;
  REQUIRE(runtime.registerApp(cadenza::apps::kLauncherAppId, app, false));
  REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  cadenza::host::DeterministicRunner runner{runtime, canvas, framebuffer,
                                             0.01F};
  cadenza::host::InputScript<3> script;
  cadenza::InputFrame first;
  first.turn = 2;
  cadenza::InputFrame third;
  third.turn = -1;
  REQUIRE(script.add(0, first));
  REQUIRE(script.add(2, third));

  runner.runFrames(4, script);
  CHECK(app.updates == 4);
  CHECK(app.turn == 1);
  CHECK(runner.frameIndex() == 4);
}

TEST_CASE("fresh HeadlessHost replays produce equal per-frame hashes") {
  cadenza::host::HeadlessHost first{cadenza::FramebufferProfile::TEmbed};
  cadenza::host::HeadlessHost second{cadenza::FramebufferProfile::TEmbed};
  cadenza::host::InputScript<2> script;
  cadenza::InputFrame click;
  click.clicked = true;
  REQUIRE(script.add(1, click));

  for (cadenza::FrameIndex frame = 0; frame < 30; ++frame) {
    first.step(script.inputAt(frame));
    second.step(script.inputAt(frame));
    CHECK(first.framebufferHash() == second.framebufferHash());
  }
  CHECK(first.runtime().currentId() == second.runtime().currentId());
}

TEST_CASE("Activation Timer completes through Menu and requires a fresh click") {
  cadenza::host::HeadlessHost host{cadenza::FramebufferProfile::TEmbed};
  REQUIRE(host.runtime().open(cadenza::apps::kClockAppId));
  host.advance(0.81F);
  REQUIRE_FALSE(host.runtime().transitioning());

  cadenza::InputFrame start;
  start.clicked = true;
  host.step(start);
  REQUIRE(host.services().snapshot().timer.state ==
          cadenza::TimerState::Running);

  cadenza::InputFrame menuHold;
  menuHold.longPressed = true;
  menuHold.heldMs = 650;
  host.step(menuHold);
  cadenza::InputFrame menuRelease;
  menuRelease.released = true;
  host.step(menuRelease);
  REQUIRE(host.runtime().systemMenuActive());

  cadenza::InputFrame heldAtExpiry;
  heldAtExpiry.heldMs = 700;
  host.advance(600.0F, heldAtExpiry);
  REQUIRE(host.services().snapshot().timer.state ==
          cadenza::TimerState::Expired);
  CHECK(host.runtime().systemSurfaces().timerAlertActive());
  CHECK_FALSE(host.runtime().systemMenuActive());
  CHECK(host.services().sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::TimerComplete);

  cadenza::InputFrame staleRelease;
  staleRelease.released = true;
  staleRelease.clicked = true;
  host.step(staleRelease);
  CHECK(host.services().snapshot().timer.state ==
        cadenza::TimerState::Expired);

  cadenza::InputFrame acknowledge;
  acknowledge.released = true;
  acknowledge.clicked = true;
  host.step(acknowledge);
  CHECK(host.services().snapshot().timer.state == cadenza::TimerState::Ready);
  CHECK(host.services().snapshot().timer.remainingMs == 10 * 60000);
  host.step();
  CHECK_FALSE(host.runtime().systemSurfaces().timerAlertActive());
}

TEST_CASE("Activation Timer supports the complete Launcher repeat workflow") {
  cadenza::host::HeadlessHost host{cadenza::FramebufferProfile::TEmbed};
  cadenza::InputFrame click;
  click.clicked = true;
  click.released = true;
  host.step(click);
  for (int frame = 0; frame < 64 && host.runtime().transitioning(); ++frame) {
    host.step();
  }
  REQUIRE(host.runtime().currentId() == cadenza::apps::kClockAppId);

  host.step(click);
  REQUIRE(host.services().snapshot().timer.state ==
          cadenza::TimerState::Running);
  cadenza::InputFrame menuHold;
  menuHold.longPressed = true;
  menuHold.heldMs = 650;
  host.step(menuHold);
  cadenza::InputFrame release;
  release.released = true;
  host.step(release);
  cadenza::InputFrame home;
  home.turn = 1;
  host.step(home);
  host.step(click);
  for (int frame = 0; frame < 64 && host.runtime().transitioning(); ++frame) {
    host.step();
  }
  REQUIRE(host.runtime().currentId() == cadenza::apps::kLauncherAppId);
  REQUIRE(host.services().snapshot().timer.state ==
          cadenza::TimerState::Running);

  host.advance(600.0F);
  REQUIRE(host.runtime().systemSurfaces().timerAlertActive());
  host.step(click);
  REQUIRE(host.services().snapshot().timer.state == cadenza::TimerState::Ready);
  host.step();
  host.step(click);
  for (int frame = 0; frame < 64 && host.runtime().transitioning(); ++frame) {
    host.step();
  }
  REQUIRE(host.runtime().currentId() == cadenza::apps::kClockAppId);
  host.step(click);
  CHECK(host.services().snapshot().timer.state ==
        cadenza::TimerState::Running);
}

TEST_CASE("Activation Timer expiry freezes an App transition") {
  cadenza::host::HeadlessHost host{cadenza::FramebufferProfile::Sharp};
  REQUIRE(host.runtime().open(cadenza::apps::kClockAppId));
  host.advance(0.81F);
  cadenza::InputFrame start;
  start.clicked = true;
  host.step(start);
  REQUIRE(host.runtime().open(cadenza::apps::kMotionAppId));
  host.advance(0.10F);
  REQUIRE(host.runtime().transitioning());
  const float beforeExpiry = host.runtime().transitionProgress();
  host.advance(600.0F);
  REQUIRE(host.runtime().systemSurfaces().timerAlertActive());
  CHECK(host.runtime().transitionProgress() == doctest::Approx(beforeExpiry));
  host.advance(1.0F);
  CHECK(host.runtime().transitionProgress() == doctest::Approx(beforeExpiry));
}

TEST_CASE("framebuffer hash is stable and pixel-sensitive at both profiles") {
  for (const cadenza::FramebufferProfile profile : {
           cadenza::FramebufferProfile::TEmbed,
           cadenza::FramebufferProfile::Sharp}) {
    cadenza::MonoFramebuffer first{profile};
    cadenza::MonoFramebuffer second{profile};
    CHECK(cadenza::host::framebufferHash(first) ==
          cadenza::host::framebufferHash(second));
    second.setPixel(second.width() - 1, second.height() - 1);
    CHECK(cadenza::host::framebufferHash(first) !=
          cadenza::host::framebufferHash(second));
  }
}

TEST_CASE("headless interaction produces deterministic PCM golden") {
  cadenza::host::HeadlessHost first{cadenza::FramebufferProfile::TEmbed};
  cadenza::host::HeadlessHost second{cadenza::FramebufferProfile::TEmbed};
  cadenza::InputFrame turn;
  turn.turn = 1;
  first.step(turn);
  second.step(turn);

  std::array<std::int16_t, 2048> a{};
  std::array<std::int16_t, 2048> b{};
  first.renderAudio(a.data(), a.size());
  second.renderAudio(b.data(), b.size());
  CHECK(a == b);
  const auto hash = cadenza::host::pcmHash(a.data(), a.size());
  CHECK(hash == 14994789996363689834ULL);
  const auto peak = *std::max_element(
      a.begin(), a.end(), [](std::int16_t left, std::int16_t right) {
        return std::abs(static_cast<int>(left)) <
               std::abs(static_cast<int>(right));
      });
  CHECK(std::abs(static_cast<int>(peak)) > 1000);
  CHECK(std::abs(static_cast<int>(peak)) < 12000);

  const auto confirm = cadenza::audio::InteractionSoundService::profile(
      cadenza::audio::SoundCue::Confirm);
  const auto back = cadenza::audio::InteractionSoundService::profile(
      cadenza::audio::SoundCue::Back);
  CHECK(confirm.startFrequencyHz < confirm.endFrequencyHz);
  CHECK(back.startFrequencyHz > back.endFrequencyHz);
}

TEST_CASE("headless WiFi adapter reaches online and cleanly stops") {
  using cadenza::system::ConnectivityActionType;
  cadenza::host::HeadlessHost host{cadenza::FramebufferProfile::TEmbed};
  host.step();

  REQUIRE(host.services().submit(
      cadenza::SystemCommand::setNetworkOnlineRequested(true)));
  host.step();
  host.step();
  host.step();

  const auto& online = host.services().snapshot().connectivity.wifi;
  CHECK(online.radio == cadenza::WiFiRadioState::Ready);
  CHECK(online.station == cadenza::WiFiStationState::Online);
  CHECK(traceContains(host.connectivity(),
                      ConnectivityActionType::StartWiFiRadio));
  CHECK(traceContains(host.connectivity(), ConnectivityActionType::ConnectWiFi));

  REQUIRE(host.services().submit(
      cadenza::SystemCommand::setNetworkOnlineRequested(false)));
  host.step();
  host.step();
  const auto& stopped = host.services().snapshot().connectivity.wifi;
  CHECK(stopped.radio == cadenza::WiFiRadioState::Off);
  CHECK(stopped.station == cadenza::WiFiStationState::Idle);
  CHECK(traceContains(host.connectivity(),
                      ConnectivityActionType::StopWiFiRadio));
}

TEST_CASE("headless WiFi adapter exposes deterministic timeout retry") {
  cadenza::host::HeadlessHost host{cadenza::FramebufferProfile::TEmbed};
  host.connectivity().setAutomaticWiFiSuccess(false);
  host.step();
  REQUIRE(host.services().submit(
      cadenza::SystemCommand::setNetworkOnlineRequested(true)));
  host.step();
  host.step();
  CHECK(host.services().snapshot().connectivity.wifi.station ==
        cadenza::WiFiStationState::Connecting);

  host.advance(10.1F);
  CHECK(host.services().snapshot().connectivity.wifi.station ==
        cadenza::WiFiStationState::RetryWait);
  CHECK(host.services().snapshot().connectivity.wifi.failure ==
        cadenza::WiFiFailure::AssociationTimeout);
}

TEST_CASE("headless Bluetooth adapter composes advertising and scanning") {
  using cadenza::system::ConnectivityActionType;
  cadenza::host::HeadlessHost host{cadenza::FramebufferProfile::TEmbed};
  host.step();
  REQUIRE(host.services().submit(
      cadenza::SystemCommand::setBluetoothAdvertisingRequested(true)));
  REQUIRE(host.services().submit(
      cadenza::SystemCommand::setBluetoothScanningRequested(true)));
  host.step();
  host.step();
  host.step();

  const auto& active = host.services().snapshot().connectivity.bluetoothLe;
  CHECK(active.radio == cadenza::BluetoothLeRadioState::Ready);
  CHECK(active.advertising);
  CHECK(active.scanning);
  CHECK(traceContains(host.connectivity(),
                      ConnectivityActionType::StartBluetoothLeAdvertising));
  CHECK(traceContains(host.connectivity(),
                      ConnectivityActionType::StartBluetoothLeScanning));

  REQUIRE(host.services().submit(
      cadenza::SystemCommand::setBluetoothAdvertisingRequested(false)));
  REQUIRE(host.services().submit(
      cadenza::SystemCommand::setBluetoothScanningRequested(false)));
  host.step();
  host.step();
  const auto& stopped = host.services().snapshot().connectivity.bluetoothLe;
  CHECK(stopped.radio == cadenza::BluetoothLeRadioState::Off);
  CHECK_FALSE(stopped.advertising);
  CHECK_FALSE(stopped.scanning);
}

TEST_CASE("headless Security 2 provisioning releases its session lease") {
  using cadenza::system::ConnectivityActionType;
  using cadenza::system::SystemResource;
  cadenza::host::HeadlessHost host{cadenza::FramebufferProfile::TEmbed};
  host.connectivity().setAutomaticProvisioningSuccess(true);
  host.step();
  REQUIRE(host.services().submit(cadenza::SystemCommand::startProvisioning()));
  host.step();
  CHECK(host.services().leases().ownerCount(
            SystemResource::ProvisioningSession) == 1);
  host.step();
  host.step();

  CHECK(host.services().snapshot().connectivity.provisioning.state ==
        cadenza::ProvisioningState::Succeeded);
  CHECK(host.services().leases().ownerCount(
            SystemResource::ProvisioningSession) == 0);
  CHECK(traceContains(host.connectivity(),
                      ConnectivityActionType::StartProvisioning));
  CHECK(traceContains(host.connectivity(),
                      ConnectivityActionType::StopProvisioning));
}

TEST_CASE("headless App transition releases only the foreground network owner") {
  using cadenza::system::SystemResource;
  cadenza::host::HeadlessHost host{cadenza::FramebufferProfile::TEmbed};
  host.step();
  REQUIRE(host.runtime().open(cadenza::apps::kSettingsAppId));
  for (int frame = 0; frame < 64 && host.runtime().transitioning(); ++frame) {
    host.step();
  }
  REQUIRE_FALSE(host.runtime().transitioning());

  const auto settingsOwner =
      cadenza::ResourceOwner::app(cadenza::apps::kSettingsAppId);
  REQUIRE(host.services().submit(
      {settingsOwner,
       cadenza::SystemCommand::setNetworkOnlineRequested(true), 0}));
  REQUIRE(host.services().submit(
      {cadenza::ResourceOwner::usb(),
       cadenza::SystemCommand::setNetworkOnlineRequested(true), 0}));
  host.step();
  CHECK(host.services().leases().ownerCount(SystemResource::Network) == 2);

  REQUIRE(host.runtime().open(cadenza::apps::kLauncherAppId));
  for (int frame = 0; frame < 64 && host.runtime().transitioning(); ++frame) {
    host.step();
  }
  REQUIRE_FALSE(host.runtime().transitioning());
  CHECK(host.services().leases().ownerCount(SystemResource::Network) == 1);
  CHECK(host.services().snapshot().connectivity.wifi.desired ==
        cadenza::WiFiDesiredPolicy::OnlineRequested);

  REQUIRE(host.services().submit(
      {cadenza::ResourceOwner::usb(),
       cadenza::SystemCommand::setNetworkOnlineRequested(false), 0}));
  host.step();
  host.step();
  CHECK_FALSE(host.services().leases().desired(SystemResource::Network));
  CHECK(host.services().snapshot().connectivity.wifi.radio ==
        cadenza::WiFiRadioState::Off);
}

TEST_CASE("headless framebuffer and action dump exclude provisioning canary") {
  constexpr const char* kSecretCanary =
      "CADENZA_SECRET_CANARY_7f9e2a_do_not_log";
  const std::string privilegedFixture{kSecretCanary};
  cadenza::host::HeadlessHost host{cadenza::FramebufferProfile::TEmbed};
  host.connectivity().setAutomaticProvisioningSuccess(true);
  host.step();
  REQUIRE(host.services().submit(cadenza::SystemCommand::startProvisioning()));
  host.step();
  host.step();
  host.step();

  std::string actionDump;
  for (std::size_t index = 0; index < host.connectivity().traceSize(); ++index) {
    const auto* action = host.connectivity().traceAt(index);
    REQUIRE(action != nullptr);
    actionDump += std::to_string(static_cast<unsigned>(action->type));
    actionDump += ':';
    actionDump += std::to_string(action->generation);
    actionDump += '\n';
  }
  const auto* bytes = host.framebuffer().data();
  const auto* bytesEnd = bytes + host.framebuffer().sizeBytes();
  const auto* canary = reinterpret_cast<const std::uint8_t*>(kSecretCanary);
  const auto* canaryEnd = canary + std::strlen(kSecretCanary);
  CHECK(std::search(bytes, bytesEnd, canary, canaryEnd) == bytesEnd);
  CHECK(actionDump.find(kSecretCanary) == std::string::npos);
  CHECK(privilegedFixture.find(kSecretCanary) != std::string::npos);
}
