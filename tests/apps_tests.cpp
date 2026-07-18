#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

#include "cadenza/apps/apps.h"
#include "app_context_test_support.h"

namespace {
bool hasBlackPixel(const cadenza::MonoFramebuffer& framebuffer) {
  return std::any_of(framebuffer.data(),
                     framebuffer.data() + framebuffer.sizeBytes(),
                     [](std::uint8_t byte) { return byte != 0; });
}

class NamedApp final : public cadenza::App {
 public:
  explicit NamedApp(const char* value) : value_(value) {}
  const char* name() const noexcept override { return value_; }
  void update(const cadenza::AppUpdateContext&) noexcept override {}
  void render(cadenza::MonoCanvas&,
              const cadenza::AppRenderContext&) noexcept override {}

 private:
  const char* value_;
};

class TextDiagnosticSink final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    if (event.message && std::strcmp(event.message, "text clipped") == 0) {
      textClipped = true;
    }
  }
  bool textClipped = false;
};

template <typename AppType>
bool registerBuiltin(cadenza::AppRuntime& runtime, cadenza::AppId id,
                     AppType& app, bool visible = true) {
  return runtime.registerApp(id, app, visible,
                             cadenza::apps::builtinAppCapabilities(id));
}

void registerNamedApps(cadenza::AppRuntime& runtime,
                       cadenza::LauncherApp& launcher, NamedApp& clock,
                       NamedApp& motion, NamedApp& settings,
                       NamedApp& gallery) {
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kLauncherAppId, launcher, false));
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kClockAppId, clock));
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kMotionAppId, motion));
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kSettingsAppId, settings));
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kGalleryAppId, gallery));
  REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));
}
}  // namespace

TEST_CASE("bundled Apps declare minimal capabilities and default deny") {
  using cadenza::AppCapability;
  for (const cadenza::AppId id : {
           cadenza::apps::kLauncherAppId, cadenza::apps::kClockAppId,
           cadenza::apps::kMotionAppId, cadenza::apps::kGalleryAppId}) {
    const auto capabilities = cadenza::apps::builtinAppCapabilities(id);
    CHECK_FALSE(capabilities.contains(AppCapability::NetworkAcquire));
    CHECK_FALSE(capabilities.contains(AppCapability::ProvisioningManage));
    CHECK_FALSE(capabilities.contains(AppCapability::VoiceAnalyzer));
  }
  const auto settings = cadenza::apps::builtinAppCapabilities(
      cadenza::apps::kSettingsAppId);
  CHECK(settings.contains(AppCapability::NetworkAcquire));
  CHECK(settings.contains(AppCapability::ProvisioningManage));
  CHECK_FALSE(settings.contains(AppCapability::VoiceAnalyzer));
  CHECK(cadenza::apps::builtinAppCapabilities(cadenza::AppId{0x7FFF}) ==
        cadenza::AppCapabilitySet{});
}

TEST_CASE("all bundled Apps render through the portable canvas at both profiles") {
  for (const cadenza::FramebufferProfile profile : {
           cadenza::FramebufferProfile::TEmbed,
           cadenza::FramebufferProfile::Sharp}) {
    cadenza::LauncherApp launcher;
    cadenza::ClockApp clock;
    cadenza::MotionApp motion;
    cadenza::SettingsApp settings;
    cadenza::AnimationGalleryApp gallery;
    cadenza::AppRuntime runtime{profile, cadenza::kCutTransition};
    cadenza::system::SystemServiceHost services;
    REQUIRE(registerBuiltin(runtime, cadenza::apps::kLauncherAppId, launcher, false));
    REQUIRE(registerBuiltin(runtime, cadenza::apps::kClockAppId, clock));
    REQUIRE(registerBuiltin(runtime, cadenza::apps::kMotionAppId, motion));
    REQUIRE(registerBuiltin(runtime, cadenza::apps::kSettingsAppId, settings));
    REQUIRE(registerBuiltin(runtime, cadenza::apps::kGalleryAppId, gallery));
    REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));

    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer};
    for (cadenza::App* app : std::array<cadenza::App*, 5>{
             &launcher, &clock, &motion, &settings, &gallery}) {
      framebuffer.clear(false);
      cadenza::test::renderApp(*app, canvas, runtime, services);
      CHECK(hasBlackPixel(framebuffer));
    }
  }
}

TEST_CASE("portable Launcher opens the selected registered App") {
  cadenza::LauncherApp launcher;
  cadenza::ClockApp clock;
  cadenza::AppRuntime runtime;
    cadenza::system::SystemServiceHost services;
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kLauncherAppId, launcher, false));
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kClockAppId, clock));
  REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));
  cadenza::InputFrame input;
  input.clicked = true;
  cadenza::test::updateRuntime(runtime, services, 1.0F / 60.0F, input);
  CHECK(runtime.transitioning());
  cadenza::test::updateRuntime(runtime, services, 0.17F);
  CHECK(runtime.currentId() == cadenza::apps::kClockAppId);
}

TEST_CASE("one input frame emits at most one semantic navigation cue") {
  cadenza::LauncherApp launcher;
  NamedApp clock{"Clock"};
  NamedApp motion{"Motion"};
  NamedApp settings{"Settings"};
  NamedApp gallery{"Gallery"};
  cadenza::AppRuntime runtime;
    cadenza::system::SystemServiceHost services;
  registerNamedApps(runtime, launcher, clock, motion, settings, gallery);

  cadenza::InputFrame turn;
  turn.turn = 7;
  cadenza::test::updateRuntime(runtime, services, 1.0F / 60.0F, turn);
  CHECK(services.sound().pendingCommandCount() == 1);
  CHECK(services.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::Navigate);
}

TEST_CASE("bundled Apps emit success and boundary cues from actual state") {
  cadenza::AppRuntime runtime;
    cadenza::system::SystemServiceHost services;
  cadenza::LauncherApp launcher;
  cadenza::ClockApp clock;
  cadenza::MotionApp motion;
  cadenza::SettingsApp settings;
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kLauncherAppId, launcher, false));
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kClockAppId, clock));
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kMotionAppId, motion));
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kSettingsAppId, settings));
  REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));

  cadenza::InputFrame click;
  click.clicked = true;
  cadenza::test::updateApp(clock, 0.0F, click, runtime, services,
                           cadenza::apps::kClockAppId);
  CHECK(services.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::ToggleOff);

  cadenza::InputFrame largeTurn;
  largeTurn.turn = 100;
  cadenza::test::updateApp(motion, 0.0F, largeTurn, runtime, services,
                           cadenza::apps::kMotionAppId);
  CHECK(services.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::Navigate);
  services.sound().advance(0.03F);
  cadenza::InputFrame boundaryTurn;
  boundaryTurn.turn = 1;
  cadenza::test::updateApp(motion, 0.0F, boundaryTurn, runtime, services,
                           cadenza::apps::kMotionAppId);
  CHECK(services.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::Boundary);
}

TEST_CASE("Settings exposes a sound row and cycles session volume") {
  cadenza::AppRuntime runtime;
    cadenza::system::SystemServiceHost services;
  cadenza::SettingsApp settings;
  cadenza::LauncherApp launcher;
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kLauncherAppId, launcher, false));
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kSettingsAppId, settings));
  REQUIRE(runtime.begin(cadenza::apps::kSettingsAppId));

  cadenza::InputFrame selectSound;
  selectSound.turn = 1;
  cadenza::test::updateApp(settings, 0.03F, selectSound, runtime, services,
                           cadenza::apps::kSettingsAppId);
  cadenza::InputFrame click;
  click.clicked = true;
  cadenza::test::updateApp(settings, 0.0F, click, runtime, services,
                           cadenza::apps::kSettingsAppId);
  CHECK(services.snapshot().soundVolume == cadenza::audio::SoundVolume::High);
  cadenza::test::updateApp(settings, 0.0F, click, runtime, services,
                           cadenza::apps::kSettingsAppId);
  CHECK(services.snapshot().soundVolume == cadenza::audio::SoundVolume::Muted);
  cadenza::test::updateApp(settings, 0.0F, click, runtime, services,
                           cadenza::apps::kSettingsAppId);
  CHECK(services.snapshot().soundVolume == cadenza::audio::SoundVolume::Low);
}

TEST_CASE("Settings volume changes are immediately visible and muted stays visual") {
  for (const cadenza::FramebufferProfile profile : {
           cadenza::FramebufferProfile::TEmbed,
           cadenza::FramebufferProfile::Sharp}) {
    CAPTURE(static_cast<int>(profile));
    cadenza::AppRuntime runtime{profile};
    cadenza::system::SystemServiceHost services;
    cadenza::SettingsApp settings;
    cadenza::LauncherApp launcher;
    REQUIRE(registerBuiltin(runtime, cadenza::apps::kLauncherAppId, launcher, false));
    REQUIRE(registerBuiltin(runtime, cadenza::apps::kSettingsAppId, settings));
    REQUIRE(runtime.begin(cadenza::apps::kSettingsAppId));
    cadenza::InputFrame selectSound;
    selectSound.turn = 1;
    cadenza::test::updateApp(settings, 0.03F, selectSound, runtime, services,
                             cadenza::apps::kSettingsAppId);

    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer};
    cadenza::test::renderApp(settings, canvas, runtime, services);
    const std::vector<std::uint8_t> medium(
        framebuffer.data(), framebuffer.data() + framebuffer.sizeBytes());

    cadenza::InputFrame click;
    click.clicked = true;
    cadenza::test::updateApp(settings, 0.0F, click, runtime, services,
                             cadenza::apps::kSettingsAppId);
    cadenza::test::renderApp(settings, canvas, runtime, services);
    const std::vector<std::uint8_t> high(
        framebuffer.data(), framebuffer.data() + framebuffer.sizeBytes());
    CHECK(services.snapshot().soundVolume == cadenza::audio::SoundVolume::High);
    CHECK(high != medium);

    cadenza::test::updateApp(settings, 0.0F, click, runtime, services,
                             cadenza::apps::kSettingsAppId);
    cadenza::test::renderApp(settings, canvas, runtime, services);
    const std::vector<std::uint8_t> muted(
        framebuffer.data(), framebuffer.data() + framebuffer.sizeBytes());
    CHECK(services.snapshot().soundVolume == cadenza::audio::SoundVolume::Muted);
    CHECK(muted != high);
    std::array<std::int16_t, 256> samples{};
    services.renderAudio(samples.data(), samples.size());
    CHECK(std::all_of(samples.begin(), samples.end(),
                      [](std::int16_t sample) { return sample == 0; }));
    CHECK(hasBlackPixel(framebuffer));
  }
}

TEST_CASE("Settings drives Wi-Fi and owner-bound provisioning without credentials") {
  cadenza::AppRuntime runtime;
  cadenza::system::SystemServiceHost services;
  cadenza::SettingsApp settings;
  cadenza::LauncherApp launcher;
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kLauncherAppId, launcher,
                          false));
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kSettingsAppId, settings));
  REQUIRE(runtime.begin(cadenza::apps::kSettingsAppId));
  services.connectivityService().setWiFiAvailable(true);

  cadenza::InputFrame selectWifi;
  selectWifi.turn = 2;
  cadenza::test::updateApp(settings, 0.0F, selectWifi, runtime, services,
                           cadenza::apps::kSettingsAppId);
  cadenza::InputFrame click;
  click.clicked = true;
  cadenza::test::updateApp(settings, 0.0F, click, runtime, services,
                           cadenza::apps::kSettingsAppId);
  CHECK(services.snapshot().connectivity.wifi.desired ==
        cadenza::WiFiDesiredPolicy::OnlineRequested);

  cadenza::InputFrame selectSetup;
  selectSetup.turn = 1;
  cadenza::test::updateApp(settings, 0.0F, selectSetup, runtime, services,
                           cadenza::apps::kSettingsAppId);
  cadenza::test::updateApp(settings, 0.0F, click, runtime, services,
                           cadenza::apps::kSettingsAppId);
  CHECK(services.snapshot().connectivity.provisioning.state ==
        cadenza::ProvisioningState::Advertising);
  CHECK(services.leases().ownerCount(
            cadenza::system::SystemResource::ProvisioningSession) == 1);

  cadenza::test::updateApp(settings, 0.0F, click, runtime, services,
                           cadenza::apps::kSettingsAppId);
  CHECK(services.snapshot().connectivity.provisioning.state ==
        cadenza::ProvisioningState::Stopping);
  const std::uint32_t cancelledGeneration =
      services.snapshot().connectivity.provisioning.generation;
  cadenza::system::ConnectivityAction action;
  while (services.connectivityService().tryPopAction(action)) {}
  REQUIRE(services.postPlatformEvent(
      cadenza::system::PlatformEvent::provisioningStopped(
          cancelledGeneration)));
  services.beginFrame(0.0F);
  cadenza::test::updateApp(settings, 0.0F, click, runtime, services,
                           cadenza::apps::kSettingsAppId);
  CHECK(services.snapshot().connectivity.provisioning.generation ==
        cancelledGeneration);
  CHECK_FALSE(services.connectivityService().tryPopAction(action));
  cadenza::test::updateApp(settings, 0.0F, click, runtime, services,
                           cadenza::apps::kSettingsAppId);
  CHECK(services.snapshot().connectivity.provisioning.generation !=
        cancelledGeneration);
  REQUIRE(services.connectivityService().tryPopAction(action));
  CHECK(action.type ==
        cadenza::system::ConnectivityActionType::ResetCredentials);
  REQUIRE(services.connectivityService().tryPopAction(action));
  CHECK(action.type ==
        cadenza::system::ConnectivityActionType::StartProvisioning);
}

TEST_CASE("Launcher contains Animation Gallery inside the decorated card") {
  for (const cadenza::FramebufferProfile profile : {
           cadenza::FramebufferProfile::TEmbed,
           cadenza::FramebufferProfile::Sharp}) {
    CAPTURE(static_cast<int>(profile));
    cadenza::LauncherApp launcher;
    NamedApp clock{"Clock"};
    NamedApp motion{"Motion"};
    NamedApp settings{"Settings"};
    NamedApp gallery{"Animation Gallery"};
    TextDiagnosticSink diagnostics;
    cadenza::AppRuntime runtime{profile, cadenza::kCutTransition};
    cadenza::system::SystemServiceHost services;
    runtime.setDiagnosticSink(&diagnostics);
    registerNamedApps(runtime, launcher, clock, motion, settings, gallery);
    cadenza::InputFrame selectGallery;
    selectGallery.turn = 3;
    cadenza::test::updateApp(launcher, 0.0F, selectGallery, runtime, services);

    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
    cadenza::test::renderApp(launcher, canvas, runtime, services);

    const int cardX = framebuffer.width() * 6 / 100;
    const int cardY = framebuffer.height() * 25 / 100;
    const int cardWidth = framebuffer.width() - cardX * 2;
    const int cardHeight = framebuffer.height() * 54 / 100;
    for (int y = cardY; y < cardY + cardHeight; ++y) {
      CHECK(framebuffer.pixel(cardX, y));
      CHECK(framebuffer.pixel(cardX + cardWidth - 1, y));
    }
    CHECK_FALSE(diagnostics.textClipped);
  }
}

TEST_CASE("Launcher footer labels never invade the navigation slot") {
  for (const cadenza::FramebufferProfile profile : {
           cadenza::FramebufferProfile::TEmbed,
           cadenza::FramebufferProfile::Sharp}) {
    CAPTURE(static_cast<int>(profile));
    cadenza::LauncherApp shortLauncher;
    NamedApp shortClock{"A"};
    NamedApp shortMotion{"B"};
    NamedApp shortSettings{"C"};
    NamedApp shortGallery{"D"};
    cadenza::AppRuntime shortRuntime{profile, cadenza::kCutTransition};
    cadenza::system::SystemServiceHost shortServices;
    registerNamedApps(shortRuntime, shortLauncher, shortClock, shortMotion,
                      shortSettings, shortGallery);
    cadenza::MonoFramebuffer shortFrame{profile};
    cadenza::MonoCanvas shortCanvas{shortFrame};
    cadenza::test::renderApp(shortLauncher, shortCanvas, shortRuntime,
                             shortServices);

    cadenza::LauncherApp longLauncher;
    NamedApp longClock{"CLOCK WITH AN EXTREMELY LONG NAME"};
    NamedApp longMotion{"MOTION WITH AN EXTREMELY LONG NAME"};
    NamedApp longSettings{"SETTINGS WITH AN EXTREMELY LONG NAME"};
    NamedApp longGallery{"GALLERY WITH AN EXTREMELY LONG NAME"};
    TextDiagnosticSink diagnostics;
    cadenza::AppRuntime longRuntime{profile, cadenza::kCutTransition};
    cadenza::system::SystemServiceHost longServices;
    longRuntime.setDiagnosticSink(&diagnostics);
    registerNamedApps(longRuntime, longLauncher, longClock, longMotion,
                      longSettings, longGallery);
    cadenza::MonoFramebuffer longFrame{profile};
    cadenza::MonoCanvas longCanvas{longFrame, &diagnostics};
    cadenza::test::renderApp(longLauncher, longCanvas, longRuntime,
                             longServices);

    const int centerX = shortFrame.width() / 2;
    const int footerY = shortFrame.height() - 17;
    for (int y = footerY - 5; y <= footerY + 5; ++y) {
      for (int x = centerX - 28; x <= centerX + 28; ++x) {
        CHECK(longFrame.pixel(x, y) == shortFrame.pixel(x, y));
      }
    }
    CHECK_FALSE(diagnostics.textClipped);
  }
}
