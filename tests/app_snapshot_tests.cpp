#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

#include "cadenza/desktop/png_writer.h"
#include "cadenza/host/headless_host.h"

namespace {
struct SnapshotCase {
  cadenza::FramebufferProfile profile;
  cadenza::AppId app;
  std::uint64_t expected;
};

enum class HandoffScene : std::uint8_t {
  ClockLaunch,
  MotionLaunch,
  SettingsLaunch,
  GalleryLaunch,
  FallbackLaunch,
  ClockReturn,
  MenuOpening,
  MenuClosing,
};

const char* handoffSceneName(HandoffScene scene) {
  switch (scene) {
    case HandoffScene::ClockLaunch:
      return "clock-launch";
    case HandoffScene::MotionLaunch:
      return "motion-launch";
    case HandoffScene::SettingsLaunch:
      return "settings-launch";
    case HandoffScene::GalleryLaunch:
      return "gallery-launch";
    case HandoffScene::FallbackLaunch:
      return "fallback-launch";
    case HandoffScene::ClockReturn:
      return "clock-return";
    case HandoffScene::MenuOpening:
      return "menu-opening";
    case HandoffScene::MenuClosing:
      return "menu-closing";
  }
  return "unknown";
}

void settle(cadenza::host::HeadlessHost& host) {
  host.advance(0.81F);
  REQUIRE_FALSE(host.runtime().transitioning());
}

void openMenu(cadenza::host::HeadlessHost& host) {
  cadenza::InputFrame held;
  held.longPressed = true;
  held.heldMs = 650;
  host.step(held);
  cadenza::InputFrame released;
  released.released = true;
  host.step(released);
  REQUIRE(host.runtime().systemMenuActive());
}

std::uint64_t captureHandoff(cadenza::FramebufferProfile profile,
                             HandoffScene scene, std::uint64_t expected) {
  class FallbackApp final : public cadenza::App {
   public:
    const char* name() const noexcept override { return "FALLBACK"; }
    void update(const cadenza::AppUpdateContext&) noexcept override {}
    void render(cadenza::MonoCanvas& canvas,
                const cadenza::AppRenderContext&) noexcept override {
      canvas.clear(false);
      canvas.text("FALLBACK APP", canvas.width() / 2,
                  canvas.height() / 2, 2, true,
                  cadenza::TextAlign::MiddleCenter);
    }
  } fallback;
  constexpr cadenza::AppId kFallbackAppId{91};
  if (scene == HandoffScene::FallbackLaunch) {
    cadenza::LauncherApp launcher;
    cadenza::AppRuntime runtime{profile};
    cadenza::system::SystemServiceHost services;
    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer};
    REQUIRE(runtime.registerApp(cadenza::apps::kLauncherAppId, launcher));
    REQUIRE(runtime.registerApp(kFallbackAppId, fallback));
    runtime.configureHome(cadenza::apps::kLauncherAppId);
    REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));
    runtime.bindSystem(services.snapshot(), services);
    REQUIRE(runtime.open(kFallbackAppId));
    runtime.updateWithSystem(0.24F, {}, services.snapshot(), services);
    runtime.renderWithSystem(canvas, services.snapshot());
    const std::uint64_t actual = cadenza::host::framebufferHash(framebuffer);
    const auto directory =
        std::filesystem::current_path() / "snapshot-failures";
    const auto path = directory /
        (std::string{"handoff-"} + handoffSceneName(scene) + "-p" +
         std::to_string(static_cast<int>(profile)) + ".png");
    if (actual != expected) {
      std::filesystem::create_directories(directory);
      CHECK(cadenza::desktop::writePng(path.string(), framebuffer));
    } else {
      std::filesystem::remove(path);
    }
    return actual;
  }
  cadenza::host::HeadlessHost host{profile};
  cadenza::AppId target = cadenza::apps::kClockAppId;
  switch (scene) {
    case HandoffScene::MotionLaunch:
      target = cadenza::apps::kMotionAppId;
      break;
    case HandoffScene::SettingsLaunch:
      target = cadenza::apps::kSettingsAppId;
      break;
    case HandoffScene::GalleryLaunch:
      target = cadenza::apps::kGalleryAppId;
      break;
    case HandoffScene::FallbackLaunch:
      break;
    default:
      break;
  }
  REQUIRE(host.runtime().open(target));
  if (scene == HandoffScene::ClockLaunch ||
      scene == HandoffScene::MotionLaunch ||
      scene == HandoffScene::SettingsLaunch ||
      scene == HandoffScene::GalleryLaunch ||
      scene == HandoffScene::FallbackLaunch) {
    host.advance(0.24F);
  } else {
    settle(host);
    if (scene == HandoffScene::ClockReturn) {
      REQUIRE(host.runtime().open(cadenza::apps::kLauncherAppId));
      host.advance(0.14F);
    } else {
      openMenu(host);
      if (scene == HandoffScene::MenuOpening) {
        host.advance(0.06F);
      } else {
        host.advance(0.20F);
        cadenza::InputFrame resume;
        resume.clicked = true;
        host.step(resume);
        host.advance(0.06F);
      }
    }
  }

  const std::uint64_t actual = host.framebufferHash();
  const auto directory = std::filesystem::current_path() / "snapshot-failures";
  const auto path = directory /
      (std::string{"handoff-"} + handoffSceneName(scene) + "-p" +
       std::to_string(static_cast<int>(profile)) + ".png");
  if (actual != expected) {
    std::filesystem::create_directories(directory);
    CHECK(cadenza::desktop::writePng(path.string(), host.framebuffer()));
  } else {
    std::filesystem::remove(path);
  }
  return actual;
}

std::uint64_t capture(cadenza::FramebufferProfile profile,
                      cadenza::AppId app, std::uint64_t expected) {
  cadenza::host::HeadlessHost host{profile};
  if (app != cadenza::apps::kLauncherAppId) {
    REQUIRE(host.runtime().open(app));
    for (int frame = 0; frame < 64 && host.runtime().transitioning(); ++frame) {
      host.step();
    }
    REQUIRE_FALSE(host.runtime().transitioning());
    REQUIRE(host.runtime().currentId() == app);
  }
  const std::uint64_t actual = host.framebufferHash();
  const auto directory = std::filesystem::current_path() / "snapshot-failures";
  const auto path = directory /
      ("app-p" + std::to_string(static_cast<int>(profile)) + "-a" +
       std::to_string(static_cast<int>(app.value())) + ".png");
  if (actual != expected) {
    std::filesystem::create_directories(directory);
    CHECK(cadenza::desktop::writePng(path.string(), host.framebuffer()));
  } else {
    std::filesystem::remove(path);
  }
  return actual;
}

std::uint64_t captureLauncherGallery(cadenza::FramebufferProfile profile,
                                     std::uint64_t expected) {
  cadenza::host::HeadlessHost host{profile};
  cadenza::InputFrame selectGallery;
  selectGallery.turn = 3;
  host.step(selectGallery);
  const std::uint64_t actual = host.framebufferHash();
  const auto directory = std::filesystem::current_path() / "snapshot-failures";
  const auto path = directory /
      ("launcher-gallery-p" + std::to_string(static_cast<int>(profile)) +
       ".png");
  if (actual != expected) {
    std::filesystem::create_directories(directory);
    CHECK(cadenza::desktop::writePng(path.string(), host.framebuffer()));
  } else {
    std::filesystem::remove(path);
  }
  return actual;
}
}  // namespace

TEST_CASE("approved bundled App framebuffer snapshots") {
  const std::array<SnapshotCase, 10> cases{{
      {cadenza::FramebufferProfile::TEmbed, cadenza::apps::kLauncherAppId,
       5609633100608380107ULL},
      {cadenza::FramebufferProfile::TEmbed, cadenza::apps::kClockAppId,
       2172376209712558838ULL},
      {cadenza::FramebufferProfile::TEmbed, cadenza::apps::kMotionAppId,
       11046562126395087774ULL},
      {cadenza::FramebufferProfile::TEmbed, cadenza::apps::kSettingsAppId,
       7412932320142494479ULL},
      {cadenza::FramebufferProfile::TEmbed, cadenza::apps::kGalleryAppId,
       14139291840108583961ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::apps::kLauncherAppId,
       17517733931075453906ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::apps::kClockAppId,
       8667913246713477979ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::apps::kMotionAppId,
       2956592992690758759ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::apps::kSettingsAppId,
       13007615703856417540ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::apps::kGalleryAppId,
       13234575752027769465ULL},
  }};

  for (const SnapshotCase& snapshot : cases) {
    CAPTURE(static_cast<int>(snapshot.profile));
    CAPTURE(static_cast<int>(snapshot.app.value()));
    CHECK(capture(snapshot.profile, snapshot.app, snapshot.expected) ==
          snapshot.expected);
  }
}

TEST_CASE("Launcher gallery selection remains bounded at both profiles") {
  CHECK(captureLauncherGallery(cadenza::FramebufferProfile::TEmbed,
                               10381184867980651961ULL) ==
        10381184867980651961ULL);
  CHECK(captureLauncherGallery(cadenza::FramebufferProfile::Sharp,
                               10395072706391661531ULL) ==
        10395072706391661531ULL);
}

TEST_CASE("approved App handoff and warped Menu keyframes") {
  constexpr std::array<HandoffScene, 8> scenes{
      HandoffScene::ClockLaunch, HandoffScene::MotionLaunch,
      HandoffScene::SettingsLaunch, HandoffScene::GalleryLaunch,
      HandoffScene::FallbackLaunch, HandoffScene::ClockReturn,
      HandoffScene::MenuOpening,
      HandoffScene::MenuClosing};
  constexpr std::array<cadenza::FramebufferProfile, 2> profiles{
      cadenza::FramebufferProfile::TEmbed,
      cadenza::FramebufferProfile::Sharp};
  constexpr std::array<std::array<std::uint64_t, 8>, 2> expected{{
      {{2978872973827686259ULL, 9220982418828621297ULL,
        1468151701910592970ULL, 14891017424807287687ULL,
        9650148204554733528ULL, 10620415418877875313ULL,
        16680032181822085337ULL,
        2789452226294592942ULL}},
      {{17805705126676167683ULL, 2438810636140423116ULL,
        381654916734470956ULL, 6285301564810435015ULL,
        9756231723368371849ULL, 18150222162577020456ULL,
        10253295035054357084ULL,
        11715812992187396753ULL}},
  }};
  for (std::size_t profileIndex = 0; profileIndex < profiles.size();
       ++profileIndex) {
    for (std::size_t sceneIndex = 0; sceneIndex < scenes.size(); ++sceneIndex) {
      CAPTURE(handoffSceneName(scenes[sceneIndex]));
      const auto actual = captureHandoff(
          profiles[profileIndex], scenes[sceneIndex],
          expected[profileIndex][sceneIndex]);
      CHECK(actual == expected[profileIndex][sceneIndex]);
    }
  }
}
