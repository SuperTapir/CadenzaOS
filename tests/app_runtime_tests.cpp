#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "cadenza/core/app_runtime.h"
#include "cadenza/core/app_catalog.h"
#include "cadenza/system/frame_coordinator.h"
#include "cadenza/system/system_service_host.h"

namespace {
constexpr cadenza::AppId kHomeAppId{1};
constexpr cadenza::AppId kClockAppId{2};
constexpr cadenza::AppId kMotionAppId{3};
constexpr cadenza::AppId kMissingAppId{4};

std::uint64_t pcmHash(const std::int16_t* samples, std::size_t count) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (std::size_t index = 0; index < count; ++index) {
    const auto value = static_cast<std::uint16_t>(samples[index]);
    hash = (hash ^ static_cast<std::uint8_t>(value & 0xFFU)) *
           1099511628211ULL;
    hash = (hash ^ static_cast<std::uint8_t>(value >> 8U)) *
           1099511628211ULL;
  }
  return hash;
}

class FakeApp final : public cadenza::App {
 public:
  FakeApp(const char* appName, std::vector<std::string>& events)
      : appName_(appName), events_(events) {}

  const char* name() const noexcept override { return appName_; }
  void onEnter() noexcept override { events_.push_back(std::string(appName_) + ":enter"); }
  void onExit() noexcept override { events_.push_back(std::string(appName_) + ":exit"); }
  void update(const cadenza::AppUpdateContext&) noexcept override {
    ++updates;
    events_.push_back(std::string(appName_) + ":update");
  }
  void render(cadenza::MonoCanvas&,
              const cadenza::AppRenderContext&) noexcept override {
    ++renders;
  }

  int updates = 0;
  int renders = 0;

 private:
  const char* appName_;
  std::vector<std::string>& events_;
};

struct Fixture {
  std::vector<std::string> events;
  FakeApp launcher{"Launcher", events};
  FakeApp clock{"Clock", events};
  FakeApp motion{"Motion", events};
  cadenza::AppRuntime runtime;
  cadenza::system::SystemServiceHost services;

  Fixture() {
    REQUIRE(runtime.registerApp(kHomeAppId, launcher, false));
    REQUIRE(runtime.registerApp(kClockAppId, clock));
    REQUIRE(runtime.registerApp(kMotionAppId, motion));
    REQUIRE(runtime.configureHome(kHomeAppId));
    runtime.bindSystem(services.snapshot(), services);
  }

  void update(cadenza::Seconds dt,
              const cadenza::InputFrame& input = {}) noexcept {
    const auto& snapshot = services.beginFrame(dt);
    runtime.updateWithSystem(dt, input, snapshot, services);
    services.commitCommands();
  }
};
}  // namespace

TEST_CASE("runtime registry preserves static IDs visibility and names") {
  Fixture fixture;
  CHECK(fixture.runtime.launcherAppCount() == 2);
  CHECK(fixture.runtime.launcherAppAt(0) == kClockAppId);
  CHECK(fixture.runtime.launcherAppAt(1) == kMotionAppId);
  CHECK(fixture.runtime.launcherAppAt(99) == kHomeAppId);
  CHECK(std::string(fixture.runtime.appName(kMotionAppId)) == "Motion");
  CHECK(std::string(fixture.runtime.appName(kMissingAppId)) == "MISSING");
  CHECK_FALSE(fixture.runtime.registerApp({}, fixture.clock));
}

TEST_CASE("AppId is a stable value type with an explicit invalid value") {
  constexpr cadenza::AppId unknownToCore{0x4321};
  static_assert(unknownToCore.valid());
  static_assert(unknownToCore.value() == 0x4321);
  static_assert(!cadenza::AppId{}.valid());

  std::vector<std::string> events;
  FakeApp app{"External", events};
  cadenza::AppRuntime runtime;
  REQUIRE(runtime.registerApp(unknownToCore, app));
  REQUIRE(runtime.begin(unknownToCore));
  runtime.update(0.01F, {});
  CHECK(app.updates == 1);
}

TEST_CASE("catalog rejects invalid duplicate and capacity without mutation") {
  std::vector<std::string> events;
  FakeApp first{"First", events};
  FakeApp second{"Second", events};
  FakeApp third{"Third", events};
  cadenza::AppCatalog catalog{2};

  cadenza::AppDescriptor missingApp;
  missingApp.id = cadenza::AppId{8};
  CHECK(catalog.registerApp(missingApp) ==
        cadenza::AppRegistrationResult::InvalidId);

  CHECK(catalog.registerApp({}, first, true) ==
        cadenza::AppRegistrationResult::InvalidId);
  CHECK(catalog.registerApp(
            cadenza::AppId{9}, first, true, "First",
            cadenza::AppCapabilitySet::fromRaw(0x80000000U)) ==
        cadenza::AppRegistrationResult::UnknownCapabilities);
  CHECK(catalog.registerApp(cadenza::AppId{10}, first, true) ==
        cadenza::AppRegistrationResult::Registered);
  CHECK(catalog.registerApp(cadenza::AppId{10}, second, false) ==
        cadenza::AppRegistrationResult::DuplicateId);
  CHECK(catalog.registerApp(cadenza::AppId{20}, second, false) ==
        cadenza::AppRegistrationResult::Registered);
  CHECK(catalog.registerApp(cadenza::AppId{30}, third, true) ==
        cadenza::AppRegistrationResult::CapacityExceeded);

  CHECK(catalog.size() == 2);
  CHECK(catalog.at(0)->id == cadenza::AppId{10});
  CHECK(catalog.at(0)->app == &first);
  CHECK(catalog.at(0)->visibleInLauncher);
  CHECK(catalog.at(0)->capabilities.empty());
  CHECK(catalog.at(1)->id == cadenza::AppId{20});
  CHECK(catalog.at(1)->app == &second);
  CHECK_FALSE(catalog.at(1)->visibleInLauncher);
}

TEST_CASE("system menu Home uses the composition root configured Home App") {
  std::vector<std::string> events;
  FakeApp home{"CustomHome", events};
  FakeApp child{"Child", events};
  cadenza::AppRuntime runtime;
  REQUIRE(runtime.registerApp(cadenza::AppId{77}, home, false));
  REQUIRE(runtime.registerApp(cadenza::AppId{88}, child));
  REQUIRE(runtime.configureHome(cadenza::AppId{77}));
  REQUIRE(runtime.begin(cadenza::AppId{88}));

  cadenza::InputFrame input;
  input.longPressed = true;
  runtime.update(0.01F, input);
  REQUIRE(runtime.systemMenuActive());
  CHECK_FALSE(runtime.transitioning());
  cadenza::InputFrame release;
  release.released = true;
  runtime.update(0.01F, release);
  cadenza::InputFrame turn;
  turn.turn = 1;
  runtime.update(0.01F, turn);
  cadenza::InputFrame click;
  click.clicked = true;
  click.released = true;
  runtime.update(0.01F, click);
  REQUIRE(runtime.transitioning());
  runtime.update(0.33F, {});
  CHECK(runtime.currentId() == cadenza::AppId{77});
  CHECK(events == std::vector<std::string>{"Child:enter", "Child:exit",
                                           "CustomHome:enter"});
}

TEST_CASE("begin enters one valid initial App exactly once") {
  Fixture fixture;
  CHECK_FALSE(fixture.runtime.begin(kMissingAppId));
  CHECK(fixture.events.empty());
  REQUIRE(fixture.runtime.begin(kHomeAppId));
  CHECK(fixture.events == std::vector<std::string>{"Launcher:enter"});
  CHECK_FALSE(fixture.runtime.begin(kClockAppId));
  CHECK(fixture.runtime.currentId() == kHomeAppId);
}

TEST_CASE("open guards current missing invalid and in-flight destinations") {
  Fixture fixture;
  REQUIRE(fixture.runtime.begin(kHomeAppId));
  CHECK_FALSE(fixture.runtime.open(kHomeAppId));
  CHECK_FALSE(fixture.runtime.open(kMissingAppId));
  CHECK_FALSE(fixture.runtime.open({}));
  REQUIRE(fixture.runtime.open(kClockAppId));
  CHECK(fixture.runtime.transitioning());
  CHECK(fixture.services.pendingCommandCount() == 1);
  CHECK_FALSE(fixture.runtime.open(kMotionAppId));
  CHECK(fixture.services.pendingCommandCount() == 1);
}

TEST_CASE("transition orders exit before enter at the midpoint") {
  Fixture fixture;
  REQUIRE(fixture.runtime.begin(kHomeAppId));
  REQUIRE(fixture.runtime.open(kClockAppId));
  fixture.update(0.15F);
  CHECK(fixture.events == std::vector<std::string>{"Launcher:enter"});
  fixture.update(0.02F);
  CHECK(fixture.events ==
        std::vector<std::string>{"Launcher:enter", "Launcher:exit", "Clock:enter"});
  CHECK(fixture.runtime.currentId() == kClockAppId);
}

TEST_CASE("only active App updates and transition input is frozen") {
  Fixture fixture;
  REQUIRE(fixture.runtime.begin(kHomeAppId));
  cadenza::InputFrame input;
  input.turn = 3;
  input.clicked = true;
  fixture.update(0.01F, input);
  CHECK(fixture.launcher.updates == 1);
  REQUIRE(fixture.runtime.open(kClockAppId));
  input.turn = 7;
  fixture.update(0.10F, input);
  fixture.update(0.23F, input);
  CHECK_FALSE(fixture.runtime.transitioning());
  CHECK(fixture.launcher.updates == 1);
  CHECK(fixture.clock.updates == 0);
  fixture.update(0.01F);
  CHECK(fixture.clock.updates == 1);
}

TEST_CASE("long press opens system menu without updating active App") {
  Fixture fixture;
  REQUIRE(fixture.runtime.begin(kClockAppId));
  cadenza::InputFrame input;
  input.longPressed = true;
  fixture.update(0.01F, input);
  CHECK(fixture.runtime.systemMenuActive());
  CHECK_FALSE(fixture.runtime.transitioning());
  CHECK(fixture.runtime.currentId() == kClockAppId);
  CHECK(fixture.clock.updates == 0);
  CHECK(fixture.services.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::Confirm);
  CHECK(fixture.services.sound().pendingCommandCount() == 1);
  fixture.update(0.17F);
  CHECK(fixture.runtime.currentId() == kClockAppId);
  CHECK(fixture.events == std::vector<std::string>{"Clock:enter"});
}

TEST_CASE("App open and system menu use approved semantic cues") {
  constexpr std::size_t kSamples = 44100;
  Fixture fixture;
  REQUIRE(fixture.runtime.begin(kHomeAppId));

  REQUIRE(fixture.runtime.open(kClockAppId));
  fixture.update(0.01F);
  CHECK(fixture.services.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::Confirm);
  std::array<std::int16_t, kSamples> pcm{};
  fixture.services.renderAudio(pcm.data(), pcm.size());
  CHECK(pcmHash(pcm.data(), pcm.size()) == 0x718BD5DCD3F36003ULL);

  fixture.update(0.32F);
  REQUIRE(fixture.runtime.currentId() == kClockAppId);
  cadenza::InputFrame input;
  input.longPressed = true;
  fixture.update(0.01F, input);
  CHECK(fixture.services.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::Confirm);

  cadenza::InputFrame released;
  released.released = true;
  fixture.update(0.01F, released);
  fixture.update(0.01F, input);
  CHECK_FALSE(fixture.runtime.systemMenuActive());
  CHECK(fixture.services.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::Back);
}

TEST_CASE("system menu freezes App lifecycle while services keep advancing") {
  Fixture fixture;
  REQUIRE(fixture.runtime.begin(kClockAppId));
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};

  cadenza::system::FrameCoordinator::runFrame(
      fixture.services, fixture.runtime, canvas, 0.01F, {});
  const int updatesBeforeMenu = fixture.clock.updates;
  const int rendersBeforeMenu = fixture.clock.renders;
  cadenza::InputFrame input;
  input.longPressed = true;
  cadenza::system::FrameCoordinator::runFrame(
      fixture.services, fixture.runtime, canvas, 0.01F, input);
  const int frozenRenderCount = fixture.clock.renders;
  CHECK(frozenRenderCount == rendersBeforeMenu);
  REQUIRE(fixture.services.postPlatformEvent(
      cadenza::system::PlatformEvent::soundOutputAvailability(true)));
  cadenza::InputFrame release;
  release.released = true;
  cadenza::system::FrameCoordinator::runFrame(
      fixture.services, fixture.runtime, canvas, 0.05F, release);
  cadenza::system::FrameCoordinator::runFrame(
      fixture.services, fixture.runtime, canvas, 0.05F, {});

  CHECK(fixture.clock.updates == updatesBeforeMenu);
  CHECK(fixture.clock.renders == frozenRenderCount);
  CHECK(fixture.services.snapshot().soundOutputAvailable);
  CHECK(fixture.events == std::vector<std::string>{"Clock:enter", "Clock:update"});
}

TEST_CASE("menu request during transition opens on stable destination") {
  Fixture fixture;
  REQUIRE(fixture.runtime.begin(kHomeAppId));
  REQUIRE(fixture.runtime.open(kClockAppId));
  cadenza::InputFrame held;
  held.longPressed = true;
  fixture.update(0.10F, held);
  CHECK(fixture.runtime.systemSurfaces().hasDeferredMenu());
  CHECK_FALSE(fixture.runtime.systemMenuActive());
  cadenza::InputFrame release;
  release.released = true;
  fixture.update(0.23F, release);
  CHECK_FALSE(fixture.runtime.transitioning());
  CHECK(fixture.runtime.currentId() == kClockAppId);
  CHECK_FALSE(fixture.runtime.systemMenuActive());
  fixture.update(0.01F);
  CHECK(fixture.runtime.systemMenuActive());
  CHECK(fixture.clock.updates == 0);
}

TEST_CASE("system menu setting action commits through typed system command") {
  Fixture fixture;
  REQUIRE(fixture.runtime.begin(kClockAppId));
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  cadenza::InputFrame held;
  held.longPressed = true;
  cadenza::system::FrameCoordinator::runFrame(
      fixture.services, fixture.runtime, canvas, 0.01F, held);
  cadenza::InputFrame release;
  release.released = true;
  cadenza::system::FrameCoordinator::runFrame(
      fixture.services, fixture.runtime, canvas, 0.01F, release);
  cadenza::InputFrame selectSound;
  selectSound.turn = 2;
  cadenza::system::FrameCoordinator::runFrame(
      fixture.services, fixture.runtime, canvas, 0.01F, selectSound);
  REQUIRE(fixture.runtime.systemMenuSelection() ==
          cadenza::presentation::SystemMenuItem::Sound);
  cadenza::InputFrame click;
  click.clicked = true;
  click.released = true;
  cadenza::system::FrameCoordinator::runFrame(
      fixture.services, fixture.runtime, canvas, 0.01F, click);
  CHECK(fixture.services.snapshot().soundVolume ==
        cadenza::audio::SoundVolume::High);
  CHECK(fixture.runtime.systemMenuActive());
}
