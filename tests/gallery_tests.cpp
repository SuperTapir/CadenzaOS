#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cstring>

#include "cadenza/core/apps.h"

namespace {

bool contains(const cadenza::MonoFramebuffer& framebuffer) {
  return std::any_of(framebuffer.data(),
                     framebuffer.data() + framebuffer.sizeBytes(),
                     [](std::uint8_t value) { return value != 0; });
}

}  // namespace

TEST_CASE("Gallery registry exposes every required presentation category") {
  cadenza::AnimationGalleryApp gallery;
  CHECK(gallery.pageCount() >= 14);
  const std::array<const char*, 14> required{{
      "EASING", "COMPOSITION", "SPRING", "DIP", "HORIZONTAL WIPE",
      "DIAGONAL WIPE", "IRIS", "VENETIAN BLINDS", "CHECKER DISSOLVE",
      "SELECTION", "CAMERA", "PARTICLES", "SPRITE STATE", "DITHER"}};
  for (const char* expected : required) {
    bool found = false;
    for (std::size_t index = 0; index < gallery.pageCount(); ++index) {
      found = found || std::strcmp(gallery.pageName(index), expected) == 0;
    }
    CHECK(found);
  }
  CHECK(std::strcmp(gallery.pageName(gallery.pageCount()), "INVALID") == 0);
}

TEST_CASE("Gallery navigation and paused knob scrub are deterministic") {
  cadenza::AnimationGalleryApp gallery;
  cadenza::AppRuntime runtime;
  gallery.onEnter();
  CHECK(gallery.currentPage() == 0);
  cadenza::InputFrame turn;
  turn.turn = -1;
  gallery.update(0.0F, turn, runtime);
  CHECK(gallery.currentPage() == gallery.pageCount() - 1);

  cadenza::InputFrame press;
  press.clicked = true;
  gallery.update(0.0F, press, runtime);
  CHECK(gallery.mode() == cadenza::GalleryMode::Scrub);
  turn.turn = 10;
  gallery.update(0.0F, turn, runtime);
  const float larger = gallery.progress();
  CHECK(larger == doctest::Approx(0.5F));
  gallery.update(5.0F, {}, runtime);
  CHECK(gallery.progress() == larger);
  turn.turn = -4;
  gallery.update(0.0F, turn, runtime);
  CHECK(gallery.progress() == doctest::Approx(0.3F));

  gallery.update(0.0F, press, runtime);
  CHECK(gallery.mode() == cadenza::GalleryMode::AutoPlay);
  gallery.update(0.1F, {}, runtime);
  CHECK(gallery.progress() > 0.3F);
  gallery.onEnter();
  CHECK(gallery.progress() == 0.0F);
  CHECK(gallery.mode() == cadenza::GalleryMode::AutoPlay);
}

TEST_CASE("Gallery is a normal App and every page renders at both profiles") {
  for (const cadenza::FramebufferProfile profile : {
           cadenza::FramebufferProfile::TEmbed,
           cadenza::FramebufferProfile::Sharp}) {
    cadenza::LauncherApp launcher;
    cadenza::AnimationGalleryApp gallery;
    cadenza::AppRuntime runtime{profile, cadenza::kCutTransition};
    REQUIRE(runtime.registerApp(cadenza::AppId::Launcher, launcher, false));
    REQUIRE(runtime.registerApp(cadenza::AppId::Gallery, gallery));
    REQUIRE(runtime.begin(cadenza::AppId::Launcher));
    REQUIRE(runtime.open(cadenza::AppId::Gallery));
    runtime.update(1.0F, {});
    CHECK(runtime.currentId() == cadenza::AppId::Gallery);

    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer};
    for (std::size_t page = 0; page < gallery.pageCount(); ++page) {
      framebuffer.clear(false);
      gallery.render(canvas, runtime);
      CHECK(contains(framebuffer));
      cadenza::InputFrame next;
      next.turn = 1;
      gallery.update(0.0F, next, runtime);
    }

    cadenza::InputFrame home;
    home.longPressed = true;
    runtime.update(0.0F, home);
    CHECK(runtime.transitioning());
  }
}

TEST_CASE("Settings motion preference propagates through runtime to Gallery") {
  cadenza::SettingsApp settings;
  cadenza::AnimationGalleryApp gallery;
  cadenza::AppRuntime runtime;
  CHECK(runtime.motionProfile() == cadenza::MotionProfile::Normal);

  cadenza::InputFrame press;
  press.clicked = true;
  settings.update(0.0F, press, runtime);
  CHECK(runtime.motionProfile() == cadenza::MotionProfile::Reduced);
  gallery.update(0.0F, {}, runtime);
  CHECK(gallery.motionProfile() == cadenza::MotionProfile::Reduced);

  settings.update(0.0F, press, runtime);
  CHECK(runtime.motionProfile() == cadenza::MotionProfile::Normal);
  gallery.update(0.0F, {}, runtime);
  CHECK(gallery.motionProfile() == cadenza::MotionProfile::Normal);
}
