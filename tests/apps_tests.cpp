#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>

#include "cadenza/core/apps.h"

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
  void update(cadenza::Seconds, const cadenza::InputFrame&,
              cadenza::AppRuntime&) noexcept override {}
  void render(cadenza::MonoCanvas&, const cadenza::AppRuntime&) noexcept override {}

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

void registerNamedApps(cadenza::AppRuntime& runtime,
                       cadenza::LauncherApp& launcher, NamedApp& clock,
                       NamedApp& motion, NamedApp& settings,
                       NamedApp& gallery) {
  REQUIRE(runtime.registerApp(cadenza::AppId::Launcher, launcher, false));
  REQUIRE(runtime.registerApp(cadenza::AppId::Clock, clock));
  REQUIRE(runtime.registerApp(cadenza::AppId::Motion, motion));
  REQUIRE(runtime.registerApp(cadenza::AppId::Settings, settings));
  REQUIRE(runtime.registerApp(cadenza::AppId::Gallery, gallery));
  REQUIRE(runtime.begin(cadenza::AppId::Launcher));
}
}  // namespace

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
    REQUIRE(runtime.registerApp(cadenza::AppId::Launcher, launcher, false));
    REQUIRE(runtime.registerApp(cadenza::AppId::Clock, clock));
    REQUIRE(runtime.registerApp(cadenza::AppId::Motion, motion));
    REQUIRE(runtime.registerApp(cadenza::AppId::Settings, settings));
    REQUIRE(runtime.registerApp(cadenza::AppId::Gallery, gallery));
    REQUIRE(runtime.begin(cadenza::AppId::Launcher));

    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer};
    for (cadenza::App* app : std::array<cadenza::App*, 5>{
             &launcher, &clock, &motion, &settings, &gallery}) {
      framebuffer.clear(false);
      app->render(canvas, runtime);
      CHECK(hasBlackPixel(framebuffer));
    }
  }
}

TEST_CASE("portable Launcher opens the selected registered App") {
  cadenza::LauncherApp launcher;
  cadenza::ClockApp clock;
  cadenza::AppRuntime runtime;
  REQUIRE(runtime.registerApp(cadenza::AppId::Launcher, launcher, false));
  REQUIRE(runtime.registerApp(cadenza::AppId::Clock, clock));
  REQUIRE(runtime.begin(cadenza::AppId::Launcher));
  cadenza::InputFrame input;
  input.clicked = true;
  runtime.update(1.0F / 60.0F, input);
  CHECK(runtime.transitioning());
  runtime.update(0.17F, {});
  CHECK(runtime.currentId() == cadenza::AppId::Clock);
}

TEST_CASE("one input frame emits at most one semantic navigation cue") {
  cadenza::LauncherApp launcher;
  NamedApp clock{"Clock"};
  NamedApp motion{"Motion"};
  NamedApp settings{"Settings"};
  NamedApp gallery{"Gallery"};
  cadenza::AppRuntime runtime;
  registerNamedApps(runtime, launcher, clock, motion, settings, gallery);

  cadenza::InputFrame turn;
  turn.turn = 7;
  runtime.update(1.0F / 60.0F, turn);
  CHECK(runtime.sound().pendingCommandCount() == 1);
  CHECK(runtime.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::Navigate);
}

TEST_CASE("bundled Apps emit success and boundary cues from actual state") {
  cadenza::AppRuntime runtime;
  cadenza::LauncherApp launcher;
  cadenza::ClockApp clock;
  cadenza::MotionApp motion;
  cadenza::SettingsApp settings;
  REQUIRE(runtime.registerApp(cadenza::AppId::Launcher, launcher, false));
  REQUIRE(runtime.registerApp(cadenza::AppId::Clock, clock));
  REQUIRE(runtime.registerApp(cadenza::AppId::Motion, motion));
  REQUIRE(runtime.registerApp(cadenza::AppId::Settings, settings));
  REQUIRE(runtime.begin(cadenza::AppId::Launcher));

  cadenza::InputFrame click;
  click.clicked = true;
  clock.update(0.0F, click, runtime);
  CHECK(runtime.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::ToggleOff);

  cadenza::InputFrame largeTurn;
  largeTurn.turn = 100;
  motion.update(0.0F, largeTurn, runtime);
  CHECK(runtime.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::Navigate);
  runtime.sound().advance(0.03F);
  cadenza::InputFrame boundaryTurn;
  boundaryTurn.turn = 1;
  motion.update(0.0F, boundaryTurn, runtime);
  CHECK(runtime.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::Boundary);
}

TEST_CASE("Settings exposes a sound row and cycles session volume") {
  cadenza::AppRuntime runtime;
  cadenza::SettingsApp settings;
  cadenza::LauncherApp launcher;
  REQUIRE(runtime.registerApp(cadenza::AppId::Launcher, launcher, false));
  REQUIRE(runtime.registerApp(cadenza::AppId::Settings, settings));
  REQUIRE(runtime.begin(cadenza::AppId::Settings));

  cadenza::InputFrame selectSound;
  selectSound.turn = 1;
  settings.update(0.03F, selectSound, runtime);
  cadenza::InputFrame click;
  click.clicked = true;
  settings.update(0.0F, click, runtime);
  CHECK(runtime.soundVolume() == cadenza::audio::SoundVolume::High);
  settings.update(0.0F, click, runtime);
  CHECK(runtime.soundVolume() == cadenza::audio::SoundVolume::Muted);
  settings.update(0.0F, click, runtime);
  CHECK(runtime.soundVolume() == cadenza::audio::SoundVolume::Low);
}

TEST_CASE("Settings volume changes are immediately visible and muted stays visual") {
  for (const cadenza::FramebufferProfile profile : {
           cadenza::FramebufferProfile::TEmbed,
           cadenza::FramebufferProfile::Sharp}) {
    CAPTURE(static_cast<int>(profile));
    cadenza::AppRuntime runtime{profile};
    cadenza::SettingsApp settings;
    cadenza::LauncherApp launcher;
    REQUIRE(runtime.registerApp(cadenza::AppId::Launcher, launcher, false));
    REQUIRE(runtime.registerApp(cadenza::AppId::Settings, settings));
    REQUIRE(runtime.begin(cadenza::AppId::Settings));
    cadenza::InputFrame selectSound;
    selectSound.turn = 1;
    settings.update(0.03F, selectSound, runtime);

    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer};
    settings.render(canvas, runtime);
    const std::vector<std::uint8_t> medium(
        framebuffer.data(), framebuffer.data() + framebuffer.sizeBytes());

    cadenza::InputFrame click;
    click.clicked = true;
    settings.update(0.0F, click, runtime);
    settings.render(canvas, runtime);
    const std::vector<std::uint8_t> high(
        framebuffer.data(), framebuffer.data() + framebuffer.sizeBytes());
    CHECK(runtime.soundVolume() == cadenza::audio::SoundVolume::High);
    CHECK(high != medium);

    settings.update(0.0F, click, runtime);
    settings.render(canvas, runtime);
    const std::vector<std::uint8_t> muted(
        framebuffer.data(), framebuffer.data() + framebuffer.sizeBytes());
    CHECK(runtime.soundVolume() == cadenza::audio::SoundVolume::Muted);
    CHECK(muted != high);
    std::array<std::int16_t, 256> samples{};
    runtime.renderAudio(samples.data(), samples.size());
    CHECK(std::all_of(samples.begin(), samples.end(),
                      [](std::int16_t sample) { return sample == 0; }));
    CHECK(hasBlackPixel(framebuffer));
  }
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
    runtime.setDiagnosticSink(&diagnostics);
    registerNamedApps(runtime, launcher, clock, motion, settings, gallery);
    cadenza::InputFrame selectGallery;
    selectGallery.turn = 3;
    launcher.update(0.0F, selectGallery, runtime);

    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
    launcher.render(canvas, runtime);

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
    registerNamedApps(shortRuntime, shortLauncher, shortClock, shortMotion,
                      shortSettings, shortGallery);
    cadenza::MonoFramebuffer shortFrame{profile};
    cadenza::MonoCanvas shortCanvas{shortFrame};
    shortLauncher.render(shortCanvas, shortRuntime);

    cadenza::LauncherApp longLauncher;
    NamedApp longClock{"CLOCK WITH AN EXTREMELY LONG NAME"};
    NamedApp longMotion{"MOTION WITH AN EXTREMELY LONG NAME"};
    NamedApp longSettings{"SETTINGS WITH AN EXTREMELY LONG NAME"};
    NamedApp longGallery{"GALLERY WITH AN EXTREMELY LONG NAME"};
    TextDiagnosticSink diagnostics;
    cadenza::AppRuntime longRuntime{profile, cadenza::kCutTransition};
    longRuntime.setDiagnosticSink(&diagnostics);
    registerNamedApps(longRuntime, longLauncher, longClock, longMotion,
                      longSettings, longGallery);
    cadenza::MonoFramebuffer longFrame{profile};
    cadenza::MonoCanvas longCanvas{longFrame, &diagnostics};
    longLauncher.render(longCanvas, longRuntime);

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
