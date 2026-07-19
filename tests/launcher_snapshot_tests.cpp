#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

#include "app_context_test_support.h"
#include "cadenza/apps/apps.h"
#include "cadenza/desktop/png_writer.h"
#include "cadenza/host/headless_host.h"

namespace {
class FallbackApp final : public cadenza::App {
 public:
  const char* name() const noexcept override { return "Signal Workshop"; }
  void update(const cadenza::AppUpdateContext&) noexcept override {}
  void render(cadenza::MonoCanvas&,
              const cadenza::AppRenderContext&) noexcept override {}
};

class GeometryDiagnostics final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    invalid = invalid || event.code == cadenza::DiagnosticCode::InvalidGeometry;
    clipped = clipped || event.code == cadenza::DiagnosticCode::ClippedGeometry ||
        event.code == cadenza::DiagnosticCode::FullyClipped;
  }
  bool invalid = false;
  bool clipped = false;
};

enum class Scene : std::uint8_t {
  VerticalClock,
  HorizontalClock,
  HorizontalMidpoint,
  VerticalMotion,
  VerticalSettings,
  VerticalGallery,
  VerticalFallback,
  VerticalPressed,
  VerticalClockLaunching,
};

const char* sceneName(Scene scene) {
  switch (scene) {
    case Scene::VerticalClock: return "vertical-clock";
    case Scene::HorizontalClock: return "horizontal-clock";
    case Scene::HorizontalMidpoint: return "horizontal-midpoint";
    case Scene::VerticalMotion: return "vertical-motion";
    case Scene::VerticalSettings: return "vertical-settings";
    case Scene::VerticalGallery: return "vertical-gallery";
    case Scene::VerticalFallback: return "vertical-fallback";
    case Scene::VerticalPressed: return "vertical-pressed";
    case Scene::VerticalClockLaunching: return "vertical-clock-launching";
  }
  return "invalid";
}

struct Fixture {
  explicit Fixture(cadenza::FramebufferProfile profile)
      : runtime(profile, cadenza::kCutTransition),
        framebuffer(profile), canvas(framebuffer, &diagnostics) {
    REQUIRE(runtime.registerApp(cadenza::apps::kLauncherAppId, launcher, false,
                                cadenza::apps::builtinAppCapabilities(
                                    cadenza::apps::kLauncherAppId)));
    REQUIRE(runtime.registerApp(cadenza::apps::kClockAppId, clock, true));
    REQUIRE(runtime.registerApp(cadenza::apps::kMotionAppId, motion, true));
    REQUIRE(runtime.registerApp(cadenza::apps::kSettingsAppId, settings, true));
    REQUIRE(runtime.registerApp(cadenza::apps::kGalleryAppId, gallery, true));
    REQUIRE(runtime.registerApp(cadenza::AppId{0x0201}, fallback, true));
    REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));
  }

  void setHorizontal() {
    REQUIRE(services.submit(cadenza::SystemCommand::setLauncherOrientation(
        cadenza::LauncherOrientation::Horizontal)));
    services.commitCommands();
  }

  void turn(int delta, float dt = 1.0F / 60.0F) {
    cadenza::InputFrame input;
    input.turn = static_cast<std::int16_t>(delta);
    cadenza::test::updateApp(launcher, dt, input, runtime, services);
  }

  void settle() {
    for (int frame = 0; frame < 240 && !launcher.settled(); ++frame) {
      cadenza::test::updateApp(launcher, 1.0F / 60.0F, {}, runtime, services);
    }
    REQUIRE(launcher.settled());
  }

  void render() {
    cadenza::test::renderApp(launcher, canvas, runtime, services);
    CHECK_FALSE(diagnostics.invalid);
    CHECK_FALSE(diagnostics.clipped);
  }

  cadenza::LauncherApp launcher;
  cadenza::ClockApp clock;
  cadenza::MotionApp motion;
  cadenza::SettingsApp settings;
  cadenza::AnimationGalleryApp gallery;
  FallbackApp fallback;
  GeometryDiagnostics diagnostics;
  cadenza::AppRuntime runtime;
  cadenza::system::SystemServiceHost services;
  cadenza::MonoFramebuffer framebuffer;
  cadenza::MonoCanvas canvas;
};

std::uint64_t capture(cadenza::FramebufferProfile profile, Scene scene,
                      std::uint64_t expected) {
  Fixture fixture{profile};
  switch (scene) {
    case Scene::VerticalClock: break;
    case Scene::HorizontalClock:
      fixture.setHorizontal();
      break;
    case Scene::HorizontalMidpoint:
      fixture.setHorizontal();
      fixture.turn(1);
      break;
    case Scene::VerticalMotion:
      fixture.turn(1);
      fixture.settle();
      break;
    case Scene::VerticalSettings:
      fixture.turn(2);
      fixture.settle();
      break;
    case Scene::VerticalGallery:
      fixture.turn(3);
      fixture.settle();
      break;
    case Scene::VerticalFallback:
      fixture.turn(4);
      fixture.settle();
      break;
    case Scene::VerticalPressed: {
      fixture.turn(1);
      fixture.settle();
      cadenza::InputFrame pressed;
      pressed.pressed = true;
      cadenza::test::updateApp(fixture.launcher, 0.0F, pressed,
                               fixture.runtime, fixture.services);
      break;
    }
    case Scene::VerticalClockLaunching: {
      cadenza::InputFrame clicked;
      clicked.clicked = true;
      cadenza::test::updateApp(fixture.launcher, 0.0F, clicked,
                               fixture.runtime, fixture.services);
      break;
    }
  }
  fixture.render();
  const std::uint64_t actual =
      cadenza::host::framebufferHash(fixture.framebuffer);
  const auto directory = std::filesystem::current_path() / "snapshot-failures";
  const auto path = directory /
      (std::string{"launcher-"} + sceneName(scene) + "-p" +
       std::to_string(static_cast<int>(profile)) + ".png");
  if (actual != expected) {
    std::filesystem::create_directories(directory);
    CHECK(cadenza::desktop::writePng(path.string(), fixture.framebuffer));
  } else {
    std::filesystem::remove(path);
  }
  return actual;
}
}  // namespace

TEST_CASE("approved Launcher track and Cover snapshots") {
  struct Case {
    cadenza::FramebufferProfile profile;
    Scene scene;
    std::uint64_t expected;
  };
  constexpr std::array<Scene, 9> scenes{
      Scene::VerticalClock, Scene::HorizontalClock,
      Scene::HorizontalMidpoint, Scene::VerticalMotion,
      Scene::VerticalSettings, Scene::VerticalGallery,
      Scene::VerticalFallback, Scene::VerticalPressed,
      Scene::VerticalClockLaunching};
  constexpr std::array<std::array<std::uint64_t, 9>, 2> expected{{
      {{14819956144027104414ULL, 10758075623605549074ULL,
        7427374337099710323ULL, 1148214755358172188ULL,
        9164569023145786634ULL, 10930115474075607069ULL,
        16573272554582944849ULL, 1148214755358172188ULL,
        14819956144027104414ULL}},
      {{16141554460486284975ULL, 10155347804930666320ULL,
        13112089065228565425ULL, 4025029914587953652ULL,
        4231162730687938791ULL, 12506082529590702106ULL,
        16113312188357867839ULL, 4025029914587953652ULL,
        16141554460486284975ULL}},
  }};
  constexpr std::array<cadenza::FramebufferProfile, 2> profiles{
      cadenza::FramebufferProfile::TEmbed,
      cadenza::FramebufferProfile::Sharp};
  for (std::size_t profileIndex = 0; profileIndex < profiles.size();
       ++profileIndex) {
    std::array<std::uint64_t, scenes.size()> actual{};
    for (std::size_t sceneIndex = 0; sceneIndex < scenes.size(); ++sceneIndex) {
      const auto profile = profiles[profileIndex];
      const auto scene = scenes[sceneIndex];
      CAPTURE(static_cast<int>(profile));
      CAPTURE(static_cast<int>(scene));
      CAPTURE(sceneName(scene));
      actual[sceneIndex] =
          capture(profile, scene, expected[profileIndex][sceneIndex]);
      CHECK(actual[sceneIndex] == expected[profileIndex][sceneIndex]);
    }
    CHECK(actual[3] == actual[7]);  // Motion: neutral == pressed.
    CHECK(actual[0] == actual[8]);  // Clock: 12:34 remains unchanged on launch.
  }
}
