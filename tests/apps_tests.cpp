#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cmath>
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

bool hasBlackInRect(const cadenza::MonoFramebuffer& framebuffer,
                    cadenza::Rect bounds) {
  for (int y = std::max(0, bounds.y);
       y < std::min<int>(framebuffer.height(), bounds.y + bounds.height); ++y) {
    for (int x = std::max(0, bounds.x);
         x < std::min<int>(framebuffer.width(), bounds.x + bounds.width); ++x) {
      if (framebuffer.pixel(x, y)) return true;
    }
  }
  return false;
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

class CoverProbeApp final : public cadenza::App {
 public:
  explicit CoverProbeApp(const char* value) : value_(value) {}
  const char* name() const noexcept override { return value_; }
  void update(const cadenza::AppUpdateContext&) noexcept override {}
  void render(cadenza::MonoCanvas&,
              const cadenza::AppRenderContext&) noexcept override {}
  bool renderLauncherCover(cadenza::MonoCanvas& canvas,
                           cadenza::Rect bounds) const noexcept override {
    ++renderCount;
    minimumWidth = std::min(minimumWidth, bounds.width);
    maximumWidth = std::max(maximumWidth, bounds.width);
    canvas.rect(bounds.x, bounds.y, bounds.width, bounds.height, true);
    return true;
  }

  mutable int renderCount = 0;
  mutable int minimumWidth = 32767;
  mutable int maximumWidth = 0;

  void resetCoverObservations() const noexcept {
    renderCount = 0;
    minimumWidth = 32767;
    maximumWidth = 0;
  }

 private:
  const char* value_;
};

class TextDiagnosticSink final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    if (event.message && std::strcmp(event.message, "text clipped") == 0) {
      textClipped = true;
    }
    if (event.code == cadenza::DiagnosticCode::ClippedGeometry ||
        event.code == cadenza::DiagnosticCode::FullyClipped) {
      unexpectedGeometryClip = true;
    }
    if (event.code == cadenza::DiagnosticCode::InvalidGeometry) {
      invalidGeometry = true;
    }
  }
  bool textClipped = false;
  bool unexpectedGeometryClip = false;
  bool invalidGeometry = false;
};

std::uint64_t framebufferHash(const cadenza::MonoFramebuffer& framebuffer) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (std::size_t index = 0; index < framebuffer.sizeBytes(); ++index) {
    hash ^= framebuffer.data()[index];
    hash *= 1099511628211ULL;
  }
  return hash;
}

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

TEST_CASE("App catalog forwards optional const Launcher Covers") {
  cadenza::LauncherApp launcher;
  NamedApp fallback{"Fallback"};
  CoverProbeApp covered{"Covered"};
  cadenza::AppRuntime runtime{cadenza::FramebufferProfile::TEmbed,
                              cadenza::kCutTransition};
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kLauncherAppId, launcher,
                          false));
  REQUIRE(runtime.registerApp(cadenza::AppId{0x0201}, fallback, true));
  REQUIRE(runtime.registerApp(cadenza::AppId{0x0202}, covered, true));
  REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));

  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  const cadenza::AppCatalogView catalog = runtime.catalogView();
  CHECK_FALSE(catalog.renderLauncherCover(
      cadenza::AppId{0x0201}, canvas, {8, 8, 80, 48}));
  CHECK(catalog.renderLauncherCover(
      cadenza::AppId{0x0202}, canvas, {8, 8, 80, 48}));
  CHECK(covered.renderCount == 1);
}

TEST_CASE("Launcher input and launch never mutate Cover pixels") {
  cadenza::LauncherApp launcher;
  CoverProbeApp covered{"Covered"};
  cadenza::AppRuntime runtime{cadenza::FramebufferProfile::TEmbed,
                              cadenza::kCutTransition};
  cadenza::system::SystemServiceHost services;
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kLauncherAppId, launcher,
                          false));
  REQUIRE(runtime.registerApp(cadenza::AppId{0x0201}, covered, true));
  REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};

  auto renderHash = [&]() {
    framebuffer.clear(false);
    cadenza::test::renderApp(launcher, canvas, runtime, services);
    return framebufferHash(framebuffer);
  };
  const std::uint64_t idleHash = renderHash();

  cadenza::InputFrame pressed;
  pressed.pressed = true;
  cadenza::test::updateApp(launcher, 0.0F, pressed, runtime, services);
  CHECK(renderHash() == idleHash);

  cadenza::InputFrame released;
  released.released = true;
  cadenza::test::updateApp(launcher, 0.0F, released, runtime, services);
  CHECK(renderHash() == idleHash);

  cadenza::InputFrame clicked;
  clicked.clicked = true;
  cadenza::test::updateApp(launcher, 0.0F, clicked, runtime, services);
  CHECK(runtime.transitioning());
  CHECK(renderHash() == idleHash);
}

TEST_CASE("Launcher clips moving Covers without changing their layout bounds") {
  cadenza::LauncherApp launcher;
  CoverProbeApp first{"First"};
  CoverProbeApp second{"Second"};
  CoverProbeApp third{"Third"};
  cadenza::AppRuntime runtime{cadenza::FramebufferProfile::TEmbed,
                              cadenza::kCutTransition};
  cadenza::system::SystemServiceHost services;
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kLauncherAppId, launcher,
                          false));
  REQUIRE(runtime.registerApp(cadenza::AppId{0x0201}, first, true));
  REQUIRE(runtime.registerApp(cadenza::AppId{0x0202}, second, true));
  REQUIRE(runtime.registerApp(cadenza::AppId{0x0203}, third, true));
  REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));
  REQUIRE(services.submit(cadenza::SystemCommand::setLauncherOrientation(
      cadenza::LauncherOrientation::Horizontal)));
  services.commitCommands();
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};

  cadenza::test::renderApp(launcher, canvas, runtime, services);
  cadenza::InputFrame turn;
  turn.turn = 1;
  cadenza::test::updateApp(launcher, 1.0F / 60.0F, turn, runtime, services);
  cadenza::test::renderApp(launcher, canvas, runtime, services);

  for (const CoverProbeApp* app : {&first, &second, &third}) {
    REQUIRE(app->renderCount > 0);
    CHECK(app->minimumWidth == app->maximumWidth);
  }
}

TEST_CASE("built-in Launcher Covers are distinct deterministic and bounded") {
  constexpr std::array<cadenza::AppId, 4> ids{
      cadenza::apps::kClockAppId, cadenza::apps::kMotionAppId,
      cadenza::apps::kSettingsAppId, cadenza::apps::kGalleryAppId};
  for (const cadenza::FramebufferProfile profile : {
           cadenza::FramebufferProfile::TEmbed,
           cadenza::FramebufferProfile::Sharp}) {
    cadenza::LauncherApp launcher;
    cadenza::ClockApp clock;
    cadenza::MotionApp motion;
    cadenza::SettingsApp settings;
    cadenza::AnimationGalleryApp gallery;
    cadenza::AppRuntime runtime{profile, cadenza::kCutTransition};
    REQUIRE(registerBuiltin(runtime, cadenza::apps::kLauncherAppId, launcher,
                            false));
    REQUIRE(registerBuiltin(runtime, cadenza::apps::kClockAppId, clock));
    REQUIRE(registerBuiltin(runtime, cadenza::apps::kMotionAppId, motion));
    REQUIRE(registerBuiltin(runtime, cadenza::apps::kSettingsAppId, settings));
    REQUIRE(registerBuiltin(runtime, cadenza::apps::kGalleryAppId, gallery));
    REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));
    const cadenza::AppCatalogView catalog = runtime.catalogView();
    std::array<std::uint64_t, 4> highlightedHashes{};
    for (std::size_t appIndex = 0; appIndex < ids.size(); ++appIndex) {
      std::uint64_t firstHash = 0;
      for (int pass = 0; pass < 2; ++pass) {
        TextDiagnosticSink diagnostics;
        cadenza::MonoFramebuffer framebuffer{profile};
        framebuffer.clear(true);
        cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
        const bool tEmbed =
            profile == cadenza::FramebufferProfile::TEmbed;
        const cadenza::Rect bounds{13, 7, tEmbed ? 280 : 350,
                                   tEmbed ? 124 : 155};
        REQUIRE(catalog.renderLauncherCover(ids[appIndex], canvas, bounds));
        for (int y = 0; y < framebuffer.height(); ++y) {
          for (int x = 0; x < framebuffer.width(); ++x) {
            const bool outside = x < bounds.x || y < bounds.y ||
                x >= bounds.x + bounds.width ||
                y >= bounds.y + bounds.height;
            if (outside) CHECK(framebuffer.pixel(x, y));
          }
        }
        CHECK_FALSE(diagnostics.invalidGeometry);
        CHECK_FALSE(diagnostics.unexpectedGeometryClip);
        const std::uint64_t hash = framebufferHash(framebuffer);
        if (pass == 0) {
          firstHash = hash;
        } else {
          CHECK(hash == firstHash);
        }
      }
      highlightedHashes[appIndex] = firstHash;
    }
    for (std::size_t first = 0; first < highlightedHashes.size(); ++first) {
      for (std::size_t second = first + 1;
           second < highlightedHashes.size(); ++second) {
        CHECK(highlightedHashes[first] != highlightedHashes[second]);
      }
    }
  }
}

TEST_CASE("built-in Cover pixels do not depend on App lifecycle state") {
  cadenza::LauncherApp launcher;
  cadenza::ClockApp clock;
  cadenza::MotionApp motion;
  cadenza::SettingsApp settings;
  cadenza::AnimationGalleryApp gallery;
  cadenza::AppRuntime runtime{cadenza::FramebufferProfile::TEmbed,
                              cadenza::kCutTransition};
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kLauncherAppId, launcher,
                          false));
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kClockAppId, clock));
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kMotionAppId, motion));
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kSettingsAppId, settings));
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kGalleryAppId, gallery));
  REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));
  const cadenza::AppCatalogView catalog = runtime.catalogView();
  const cadenza::Rect bounds{20, 20, 280, 124};
  const std::array<cadenza::AppId, 4> ids{
      cadenza::apps::kClockAppId, cadenza::apps::kMotionAppId,
      cadenza::apps::kSettingsAppId, cadenza::apps::kGalleryAppId};
  const std::array<cadenza::App*, 4> apps{
      &clock, &motion, &settings, &gallery};
  for (std::size_t index = 0; index < ids.size(); ++index) {
    const cadenza::AppId id = ids[index];
    cadenza::MonoFramebuffer before{cadenza::FramebufferProfile::TEmbed};
    cadenza::MonoFramebuffer after{cadenza::FramebufferProfile::TEmbed};
    cadenza::MonoCanvas beforeCanvas{before};
    cadenza::MonoCanvas afterCanvas{after};
    REQUIRE(catalog.renderLauncherCover(id, beforeCanvas, bounds));
    cadenza::App* app = apps[index];
    app->onEnter();
    cadenza::InputFrame input;
    input.turn = 1;
    input.clicked = true;
    cadenza::system::SystemServiceHost services;
    cadenza::test::updateApp(*app, 0.25F, input, runtime, services);
    REQUIRE(catalog.renderLauncherCover(id, afterCanvas, bounds));
    for (int y = bounds.y; y < bounds.y + bounds.height; ++y) {
      for (int x = bounds.x; x < bounds.x + bounds.width; ++x) {
        CHECK(before.pixel(x, y) == after.pixel(x, y));
      }
    }
  }
}

TEST_CASE("Launcher selection moves through a continuous unbounded track") {
  cadenza::LauncherApp launcher;
  NamedApp clock{"Clock"};
  NamedApp motion{"Motion"};
  NamedApp settings{"Settings"};
  NamedApp gallery{"Gallery"};
  cadenza::AppRuntime runtime;
  cadenza::system::SystemServiceHost services;
  registerNamedApps(runtime, launcher, clock, motion, settings, gallery);

  cadenza::InputFrame turn;
  turn.turn = 1;
  cadenza::test::updateApp(launcher, 1.0F / 60.0F, turn, runtime, services);
  CHECK(launcher.selectedIndex() == 1);
  CHECK(launcher.targetPosition() == 1);
  CHECK(launcher.visualPosition() > 0.0F);
  CHECK(launcher.visualPosition() < 1.0F);

  const float beforeRetarget = launcher.visualPosition();
  turn.turn = -1;
  cadenza::test::updateApp(launcher, 0.0F, turn, runtime, services);
  CHECK(launcher.selectedIndex() == 0);
  CHECK(launcher.targetPosition() == 0);
  CHECK(launcher.visualPosition() == doctest::Approx(beforeRetarget));
}

TEST_CASE("Launcher loop boundaries move one logical card in input direction") {
  cadenza::LauncherApp launcher;
  NamedApp clock{"Clock"};
  NamedApp motion{"Motion"};
  NamedApp settings{"Settings"};
  NamedApp gallery{"Gallery"};
  cadenza::AppRuntime runtime;
  cadenza::system::SystemServiceHost services;
  registerNamedApps(runtime, launcher, clock, motion, settings, gallery);

  cadenza::InputFrame toLast;
  toLast.turn = 3;
  cadenza::test::updateApp(launcher, 0.0F, toLast, runtime, services);
  CHECK(launcher.selectedIndex() == 3);
  CHECK(launcher.targetPosition() == 3);

  cadenza::InputFrame forward;
  forward.turn = 1;
  cadenza::test::updateApp(launcher, 0.0F, forward, runtime, services);
  CHECK(launcher.selectedIndex() == 0);
  CHECK(launcher.targetPosition() == 4);

  cadenza::InputFrame backward;
  backward.turn = -1;
  cadenza::test::updateApp(launcher, 0.0F, backward, runtime, services);
  CHECK(launcher.selectedIndex() == 3);
  CHECK(launcher.targetPosition() == 3);
}

TEST_CASE("Launcher opens the latest logical selection before motion settles") {
  cadenza::LauncherApp launcher;
  NamedApp clock{"Clock"};
  NamedApp motion{"Motion"};
  NamedApp settings{"Settings"};
  NamedApp gallery{"Gallery"};
  cadenza::AppRuntime runtime;
  cadenza::system::SystemServiceHost services;
  registerNamedApps(runtime, launcher, clock, motion, settings, gallery);

  cadenza::InputFrame turn;
  turn.turn = 1;
  cadenza::test::updateRuntime(runtime, services, 1.0F / 60.0F, turn);
  REQUIRE_FALSE(launcher.settled());
  cadenza::InputFrame click;
  click.clicked = true;
  cadenza::test::updateRuntime(runtime, services, 0.0F, click);
  REQUIRE(runtime.transitioning());
  cadenza::test::updateRuntime(runtime, services, 0.17F);
  CHECK(runtime.currentId() == cadenza::apps::kMotionAppId);
}

TEST_CASE("empty Launcher ignores navigation and open safely") {
  cadenza::LauncherApp launcher;
  cadenza::AppRuntime runtime;
  cadenza::system::SystemServiceHost services;
  REQUIRE(registerBuiltin(runtime, cadenza::apps::kLauncherAppId, launcher,
                          false));
  REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));

  cadenza::InputFrame input;
  input.turn = 12;
  input.clicked = true;
  cadenza::test::updateRuntime(runtime, services, 1.0F / 60.0F, input);
  CHECK_FALSE(runtime.transitioning());
  CHECK(launcher.selectedIndex() == 0);
  CHECK(launcher.targetPosition() == 0);
  CHECK(std::isfinite(launcher.visualPosition()));
}

TEST_CASE("Launcher motion profiles stay continuous with bounded behavior") {
  cadenza::LauncherApp launcher;
  NamedApp clock{"Clock"};
  NamedApp motion{"Motion"};
  NamedApp settings{"Settings"};
  NamedApp gallery{"Gallery"};
  cadenza::AppRuntime runtime;
  cadenza::system::SystemServiceHost services;
  registerNamedApps(runtime, launcher, clock, motion, settings, gallery);

  cadenza::InputFrame turn;
  turn.turn = 1;
  cadenza::test::updateApp(launcher, 1.0F / 60.0F, turn, runtime, services);
  float maximum = launcher.visualPosition();
  for (int frame = 0; frame < 120; ++frame) {
    cadenza::test::updateApp(launcher, 1.0F / 60.0F, {}, runtime, services);
    maximum = std::max(maximum, launcher.visualPosition());
  }
  CHECK(maximum > 1.0F);
  CHECK(maximum < 1.2F);
  CHECK(launcher.settled());
  CHECK(launcher.visualPosition() == doctest::Approx(1.0F));

  REQUIRE(services.submit(cadenza::SystemCommand::setMotionProfile(
      cadenza::MotionProfile::Reduced)));
  services.commitCommands();
  cadenza::test::updateApp(launcher, 0.0F, {}, runtime, services);
  turn.turn = 1;
  cadenza::test::updateApp(launcher, 1.0F / 60.0F, turn, runtime, services);
  float previous = launcher.visualPosition();
  CHECK(previous > 1.0F);
  CHECK(previous < 2.0F);
  for (int frame = 0; frame < 120; ++frame) {
    cadenza::test::updateApp(launcher, 1.0F / 60.0F, {}, runtime, services);
    CHECK(launcher.visualPosition() >= previous);
    CHECK(launcher.visualPosition() <= 2.0F);
    previous = launcher.visualPosition();
  }
  CHECK(launcher.settled());
  CHECK(launcher.visualPosition() == doctest::Approx(2.0F));
}

TEST_CASE("Launcher motion profile switch and long-run rebase do not jump") {
  cadenza::LauncherApp launcher;
  NamedApp clock{"Clock"};
  NamedApp motion{"Motion"};
  NamedApp settings{"Settings"};
  NamedApp gallery{"Gallery"};
  cadenza::AppRuntime runtime;
  cadenza::system::SystemServiceHost services;
  registerNamedApps(runtime, launcher, clock, motion, settings, gallery);

  cadenza::InputFrame turn;
  turn.turn = 1;
  cadenza::test::updateApp(launcher, 1.0F / 60.0F, turn, runtime, services);
  const float beforeProfileSwitch = launcher.visualPosition();
  REQUIRE(services.submit(cadenza::SystemCommand::setMotionProfile(
      cadenza::MotionProfile::Reduced)));
  services.commitCommands();
  cadenza::test::updateApp(launcher, 0.0F, {}, runtime, services);
  CHECK(launcher.visualPosition() == doctest::Approx(beforeProfileSwitch));

  cadenza::InputFrame manyTurns;
  manyTurns.turn = 4096;
  cadenza::test::updateApp(launcher, 2.0F, manyTurns, runtime, services);
  for (int frame = 0; frame < 240 && !launcher.settled(); ++frame) {
    cadenza::test::updateApp(launcher, 1.0F / 60.0F, {}, runtime, services);
  }
  CHECK(launcher.settled());
  CHECK(launcher.selectedIndex() == 1);
  CHECK(launcher.targetPosition() == 1);
  CHECK(launcher.visualPosition() == doctest::Approx(1.0F));
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

TEST_CASE("Settings switches Launcher orientation immediately at both profiles") {
  for (const cadenza::FramebufferProfile profile : {
           cadenza::FramebufferProfile::TEmbed,
           cadenza::FramebufferProfile::Sharp}) {
    CAPTURE(static_cast<int>(profile));
    cadenza::AppRuntime runtime{profile, cadenza::kCutTransition};
    cadenza::system::SystemServiceHost services;
    cadenza::SettingsApp settings;
    cadenza::LauncherApp launcher;
    REQUIRE(registerBuiltin(runtime, cadenza::apps::kLauncherAppId, launcher,
                            false));
    REQUIRE(registerBuiltin(runtime, cadenza::apps::kSettingsAppId, settings));
    REQUIRE(runtime.begin(cadenza::apps::kSettingsAppId));

    cadenza::InputFrame selectLauncher;
    selectLauncher.turn = 4;
    cadenza::test::updateApp(settings, 0.0F, selectLauncher, runtime, services,
                             cadenza::apps::kSettingsAppId);
    cadenza::InputFrame click;
    click.clicked = true;
    cadenza::test::updateApp(settings, 0.0F, click, runtime, services,
                             cadenza::apps::kSettingsAppId);
    CHECK(services.snapshot().launcherOrientation ==
          cadenza::LauncherOrientation::Horizontal);
    CHECK(services.sound().lastAcceptedCue() ==
          cadenza::audio::SoundCue::ToggleOn);

    TextDiagnosticSink diagnostics;
    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
    cadenza::test::renderApp(settings, canvas, runtime, services);
    CHECK(hasBlackPixel(framebuffer));
    CHECK_FALSE(diagnostics.textClipped);

    cadenza::test::updateApp(settings, 0.0F, click, runtime, services,
                             cadenza::apps::kSettingsAppId);
    CHECK(services.snapshot().launcherOrientation ==
          cadenza::LauncherOrientation::Vertical);
    CHECK(services.sound().lastAcceptedCue() ==
          cadenza::audio::SoundCue::ToggleOff);
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

TEST_CASE("Launcher settles Animation Gallery inside the decorated card") {
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
    for (int frame = 0; frame < 180 && !launcher.settled(); ++frame) {
      cadenza::test::updateApp(launcher, 1.0F / 60.0F, {}, runtime, services);
    }
    REQUIRE(launcher.settled());

    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
    cadenza::test::renderApp(launcher, canvas, runtime, services);

    const int contentWidth = std::min(350, framebuffer.width() * 7 / 8);
    const int cardWidth = contentWidth + 6;
    const int cardHeight = contentWidth * 155 / 350 + 6;
    const int cardX = framebuffer.width() / 2 - cardWidth / 2;
    const int cardY = framebuffer.height() / 2 - cardHeight / 2;
    for (int y = cardY; y < cardY + cardHeight; ++y) {
      CHECK(framebuffer.pixel(cardX, y));
      CHECK(framebuffer.pixel(cardX + cardWidth - 1, y));
    }
    CHECK_FALSE(diagnostics.textClipped);
    CHECK_FALSE(diagnostics.invalidGeometry);
    CHECK_FALSE(diagnostics.unexpectedGeometryClip);
  }
}

TEST_CASE("Launcher card labels never invade the navigation slot") {
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
    CHECK_FALSE(diagnostics.invalidGeometry);
  }
}

TEST_CASE("Launcher directions expose neighbors and intermediate motion") {
  for (const cadenza::FramebufferProfile profile : {
           cadenza::FramebufferProfile::TEmbed,
           cadenza::FramebufferProfile::Sharp}) {
    CAPTURE(static_cast<int>(profile));
    cadenza::LauncherApp launcher;
    NamedApp clock{"CLOCK WITH AN EXTREMELY LONG NAME"};
    NamedApp motion{"Motion"};
    NamedApp settings{"Settings"};
    NamedApp gallery{"Gallery"};
    cadenza::AppRuntime runtime{profile, cadenza::kCutTransition};
    cadenza::system::SystemServiceHost services;
    registerNamedApps(runtime, launcher, clock, motion, settings, gallery);
    TextDiagnosticSink diagnostics;
    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer, &diagnostics};

    cadenza::test::renderApp(launcher, canvas, runtime, services);
    const std::vector<std::uint8_t> vertical(
        framebuffer.data(), framebuffer.data() + framebuffer.sizeBytes());
    const int headerHeight = framebuffer.height() < 200 ? 22 : 27;
    CHECK(hasBlackInRect(framebuffer,
                         {0, headerHeight + 2, framebuffer.width(), 20}));
    CHECK(hasBlackInRect(framebuffer,
                         {0, framebuffer.height() - 20,
                          framebuffer.width(), 20}));

    cadenza::InputFrame turn;
    turn.turn = 1;
    cadenza::test::updateApp(launcher, 1.0F / 60.0F, turn, runtime, services);
    cadenza::test::renderApp(launcher, canvas, runtime, services);
    const std::vector<std::uint8_t> midpoint(
        framebuffer.data(), framebuffer.data() + framebuffer.sizeBytes());
    CHECK(midpoint != vertical);
    for (int frame = 0; frame < 180 && !launcher.settled(); ++frame) {
      cadenza::test::updateApp(launcher, 1.0F / 60.0F, {}, runtime, services);
    }
    cadenza::test::renderApp(launcher, canvas, runtime, services);
    const std::vector<std::uint8_t> settled(
        framebuffer.data(), framebuffer.data() + framebuffer.sizeBytes());
    CHECK(settled != midpoint);

    REQUIRE(services.submit(cadenza::SystemCommand::setLauncherOrientation(
        cadenza::LauncherOrientation::Horizontal)));
    services.commitCommands();
    cadenza::test::renderApp(launcher, canvas, runtime, services);
    const std::vector<std::uint8_t> horizontal(
        framebuffer.data(), framebuffer.data() + framebuffer.sizeBytes());
    CHECK(horizontal != settled);
    CHECK(hasBlackInRect(framebuffer,
                         {0, headerHeight + 2, 24,
                          framebuffer.height() - headerHeight - 2}));
    CHECK(hasBlackInRect(framebuffer,
                         {framebuffer.width() - 24, headerHeight + 2, 24,
                          framebuffer.height() - headerHeight - 2}));
    CHECK_FALSE(diagnostics.textClipped);
    CHECK_FALSE(diagnostics.invalidGeometry);
    CHECK_FALSE(diagnostics.unexpectedGeometryClip);
  }
}

TEST_CASE("settled Launcher framebuffer is independent of elapsed time") {
  cadenza::LauncherApp launcher;
  NamedApp clock{"Clock"};
  NamedApp motion{"Motion"};
  NamedApp settings{"Settings"};
  NamedApp gallery{"Gallery"};
  cadenza::AppRuntime runtime;
  cadenza::system::SystemServiceHost services;
  registerNamedApps(runtime, launcher, clock, motion, settings, gallery);
  for (int frame = 0; frame < 180 && !launcher.settled(); ++frame) {
    cadenza::test::updateApp(launcher, 1.0F / 60.0F, {}, runtime, services);
  }
  REQUIRE(launcher.settled());
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  cadenza::test::renderApp(launcher, canvas, runtime, services);
  const std::vector<std::uint8_t> before(
      framebuffer.data(), framebuffer.data() + framebuffer.sizeBytes());
  cadenza::test::updateApp(launcher, 0.5F, {}, runtime, services);
  cadenza::test::renderApp(launcher, canvas, runtime, services);
  const std::vector<std::uint8_t> after(
      framebuffer.data(), framebuffer.data() + framebuffer.sizeBytes());
  CHECK(after == before);
}
