#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>

#include "cadenza/core/apps.h"

namespace {
bool hasBlackPixel(const cadenza::MonoFramebuffer& framebuffer) {
  return std::any_of(framebuffer.data(),
                     framebuffer.data() + framebuffer.sizeBytes(),
                     [](std::uint8_t byte) { return byte != 0; });
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
