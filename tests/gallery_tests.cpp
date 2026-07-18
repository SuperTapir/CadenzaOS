#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>

#include "cadenza/core/apps.h"

namespace {

bool contains(const cadenza::MonoFramebuffer& framebuffer) {
  return std::any_of(framebuffer.data(),
                     framebuffer.data() + framebuffer.sizeBytes(),
                     [](std::uint8_t value) { return value != 0; });
}

std::uint64_t renderHash(cadenza::AnimationGalleryApp& gallery,
                         const cadenza::AppRuntime& runtime) {
  cadenza::MonoFramebuffer framebuffer{
      cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  gallery.render(canvas, runtime);
  std::uint64_t hash = 1469598103934665603ULL;
  for (std::size_t index = 0; index < framebuffer.sizeBytes(); ++index) {
    hash ^= framebuffer.data()[index];
    hash *= 1099511628211ULL;
  }
  return hash;
}

std::uint64_t renderContentHash(cadenza::AnimationGalleryApp& gallery,
                                const cadenza::AppRuntime& runtime) {
  cadenza::MonoFramebuffer framebuffer{
      cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  gallery.render(canvas, runtime);
  std::uint64_t hash = 1469598103934665603ULL;
  for (int y = 45; y < framebuffer.height() - 20; ++y) {
    for (int x = 0; x < framebuffer.width(); ++x) {
      hash ^= framebuffer.pixel(x, y) ? 1U : 0U;
      hash *= 1099511628211ULL;
    }
  }
  return hash;
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

TEST_CASE("Gallery transient demos replay at the same phase every cycle") {
  constexpr cadenza::Seconds kSamplePhase = 0.10F;
  constexpr cadenza::Seconds kFullCycle =
      cadenza::presentation_defaults::kGalleryCycleDuration;
  const std::array<std::int16_t, 4> transientPages{{2, 9, 10, 11}};

  for (const std::int16_t page : transientPages) {
    cadenza::AnimationGalleryApp gallery;
    cadenza::AppRuntime runtime;
    gallery.onEnter();
    cadenza::InputFrame navigate;
    navigate.turn = page;
    gallery.update(0.0F, navigate, runtime);

    gallery.update(kSamplePhase, {}, runtime);
    const float firstProgress = gallery.progress();
    const std::uint64_t firstCycle = renderHash(gallery, runtime);
    gallery.update(kFullCycle, {}, runtime);
    const std::uint64_t secondCycle = renderHash(gallery, runtime);

    CAPTURE(page);
    CHECK(gallery.progress() == doctest::Approx(
                                    kSamplePhase / kFullCycle));
    CHECK(gallery.progress() == firstProgress);
    CHECK(secondCycle == firstCycle);
  }
}

TEST_CASE("Gallery particle burst is centered in both display profiles") {
  for (const cadenza::FramebufferProfile profile : {
           cadenza::FramebufferProfile::TEmbed,
           cadenza::FramebufferProfile::Sharp}) {
    cadenza::AnimationGalleryApp gallery;
    cadenza::AppRuntime runtime{profile};
    gallery.onEnter();
    cadenza::InputFrame navigate;
    navigate.turn = 11;
    gallery.update(0.0F, navigate, runtime);

    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer};
    gallery.render(canvas, runtime);
    int minX = framebuffer.width();
    int maxX = -1;
    int minY = framebuffer.height();
    int maxY = -1;
    for (int y = 48; y < framebuffer.height() - 36; ++y) {
      for (int x = 0; x < framebuffer.width(); ++x) {
        if (!framebuffer.pixel(x, y)) continue;
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        minY = std::min(minY, y);
        maxY = std::max(maxY, y);
      }
    }

    REQUIRE(maxX >= minX);
    REQUIRE(maxY >= minY);
    CHECK((minX + maxX) / 2 == framebuffer.width() / 2);
    CHECK((minY + maxY) / 2 == framebuffer.height() / 2);
  }
}

TEST_CASE("Gallery stateful demos scrub visibly and reversibly") {
  const std::array<std::int16_t, 4> statefulPages{{2, 9, 10, 11}};
  for (const std::int16_t page : statefulPages) {
    cadenza::AppRuntime runtime;
    cadenza::AnimationGalleryApp reversed;
    reversed.onEnter();
    cadenza::InputFrame navigate;
    navigate.turn = page;
    reversed.update(0.0F, navigate, runtime);
    const std::uint64_t atStart = renderContentHash(reversed, runtime);

    cadenza::InputFrame press;
    press.clicked = true;
    reversed.update(0.0F, press, runtime);
    cadenza::InputFrame turn;
    turn.turn = 5;
    reversed.update(0.0F, turn, runtime);
    turn.turn = -4;
    reversed.update(0.0F, turn, runtime);
    const std::uint64_t afterReverse = renderContentHash(reversed, runtime);

    cadenza::AnimationGalleryApp direct;
    direct.onEnter();
    direct.update(0.0F, navigate, runtime);
    direct.update(0.0F, press, runtime);
    turn.turn = 1;
    direct.update(0.0F, turn, runtime);
    const std::uint64_t directSeek = renderContentHash(direct, runtime);

    CAPTURE(page);
    CHECK(afterReverse != atStart);
    CHECK(afterReverse == directSeek);
  }
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
