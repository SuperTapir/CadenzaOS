#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
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

std::size_t changedPixels(const cadenza::MonoFramebuffer& first,
                          const cadenza::MonoFramebuffer& second) {
  std::size_t changed = 0;
  for (std::size_t index = 0; index < first.sizeBytes(); ++index) {
    std::uint8_t bits = static_cast<std::uint8_t>(
        first.data()[index] ^ second.data()[index]);
    while (bits != 0) {
      changed += bits & 1U;
      bits >>= 1U;
    }
  }
  return changed;
}

bool sameFrame(const cadenza::MonoFramebuffer& first,
               const cadenza::MonoFramebuffer& second) {
  return first.width() == second.width() &&
         first.height() == second.height() &&
         std::equal(first.data(), first.data() + first.sizeBytes(),
                    second.data());
}

bool sameRect(const cadenza::MonoFramebuffer& first,
              const cadenza::MonoFramebuffer& second,
              cadenza::Rect bounds) {
  for (int y = bounds.y; y < bounds.y + bounds.height; ++y) {
    for (int x = bounds.x; x < bounds.x + bounds.width; ++x) {
      if (first.pixel(x, y) != second.pixel(x, y)) return false;
    }
  }
  return true;
}

enum class HandoffScene : std::uint8_t {
  TimerLaunch,
  MotionLaunch,
  SettingsLaunch,
  GalleryLaunch,
  FallbackLaunch,
  TimerReturn,
  MenuOpening,
  MenuClosing,
};

const char* handoffSceneName(HandoffScene scene) {
  switch (scene) {
    case HandoffScene::TimerLaunch:
      return "timer-launch";
    case HandoffScene::MotionLaunch:
      return "motion-launch";
    case HandoffScene::SettingsLaunch:
      return "settings-launch";
    case HandoffScene::GalleryLaunch:
      return "gallery-launch";
    case HandoffScene::FallbackLaunch:
      return "fallback-launch";
    case HandoffScene::TimerReturn:
      return "timer-return";
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
  cadenza::AppId target = cadenza::apps::kTimerAppId;
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
  if (scene == HandoffScene::TimerLaunch ||
      scene == HandoffScene::MotionLaunch ||
      scene == HandoffScene::SettingsLaunch ||
      scene == HandoffScene::GalleryLaunch ||
      scene == HandoffScene::FallbackLaunch) {
    host.advance(0.24F);
  } else {
    settle(host);
    if (scene == HandoffScene::TimerReturn) {
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
       14918215311978361535ULL},
      {cadenza::FramebufferProfile::TEmbed, cadenza::apps::kTimerAppId,
       9459123157712897673ULL},
      {cadenza::FramebufferProfile::TEmbed, cadenza::apps::kMotionAppId,
       2528157750090777159ULL},
      {cadenza::FramebufferProfile::TEmbed, cadenza::apps::kSettingsAppId,
       747746471881827462ULL},
      {cadenza::FramebufferProfile::TEmbed, cadenza::apps::kGalleryAppId,
       5829755783398426753ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::apps::kLauncherAppId,
       14894746194681732842ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::apps::kTimerAppId,
       11125504611431129214ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::apps::kMotionAppId,
       17137373998554346811ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::apps::kSettingsAppId,
       12548109968953038253ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::apps::kGalleryAppId,
       2918520654623268197ULL},
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
                               15896753282562615363ULL) ==
        15896753282562615363ULL);
  CHECK(captureLauncherGallery(cadenza::FramebufferProfile::Sharp,
                               13499352535080939587ULL) ==
        13499352535080939587ULL);
}

TEST_CASE("approved App handoff and warped Menu keyframes") {
  constexpr std::array<HandoffScene, 8> scenes{
      HandoffScene::TimerLaunch, HandoffScene::MotionLaunch,
      HandoffScene::SettingsLaunch, HandoffScene::GalleryLaunch,
      HandoffScene::FallbackLaunch, HandoffScene::TimerReturn,
      HandoffScene::MenuOpening,
      HandoffScene::MenuClosing};
  constexpr std::array<cadenza::FramebufferProfile, 2> profiles{
      cadenza::FramebufferProfile::TEmbed,
      cadenza::FramebufferProfile::Sharp};
  constexpr std::array<std::array<std::uint64_t, 8>, 2> expected{{
      {{10363111933382688924ULL, 7816090857312221288ULL,
        13616107464272084726ULL, 5062836223768158047ULL,
        8089027038418072656ULL, 17578902866895419590ULL,
        16216091776987611102ULL,
        12224806840153236890ULL}},
      {{5668728059796249764ULL, 8216619787726592616ULL,
        7999527108005928230ULL, 15678527797637150066ULL,
        14975481694991952360ULL, 10760001046756445264ULL,
        926094830975975231ULL,
        10499074208059608780ULL}},
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

TEST_CASE("built-in returns keep the Cover fixed and bound 30 FPS changes") {
  constexpr std::array<cadenza::FramebufferProfile, 2> profiles{
      cadenza::FramebufferProfile::TEmbed,
      cadenza::FramebufferProfile::Sharp};
  constexpr std::array<cadenza::AppId, 4> apps{
      cadenza::apps::kTimerAppId, cadenza::apps::kMotionAppId,
      cadenza::apps::kSettingsAppId, cadenza::apps::kGalleryAppId};
  constexpr float kMaximumChangedRatio = 0.16F;

  for (const cadenza::FramebufferProfile profile : profiles) {
    for (std::size_t appIndex = 0; appIndex < apps.size(); ++appIndex) {
      CAPTURE(static_cast<int>(profile));
      CAPTURE(appIndex);
      cadenza::host::HeadlessHost host{profile};
      if (appIndex > 0) {
        cadenza::InputFrame select;
        select.turn = static_cast<std::int16_t>(appIndex);
        host.step(select);
        host.advance(0.5F);
      }
      REQUIRE(host.runtime().open(apps[appIndex]));
      settle(host);
      const cadenza::MonoFramebuffer appFrame = host.framebuffer();

      REQUIRE(host.runtime().open(cadenza::apps::kLauncherAppId));
      host.advance(0.0F);
      CHECK(sameFrame(host.framebuffer(), appFrame));

      const int coverWidth = std::min(350, host.framebuffer().width() * 7 / 8);
      const int coverHeight = coverWidth * 155 / 350;
      const cadenza::Rect coverBounds{
          (host.framebuffer().width() - coverWidth) / 2,
          (host.framebuffer().height() - coverHeight + 1) / 2,
          coverWidth, coverHeight};
      cadenza::MonoFramebuffer cover{profile};
      cadenza::MonoCanvas coverCanvas{cover};
      cover.clear(true);
      REQUIRE(host.runtime().catalogView().renderLauncherCover(
          apps[appIndex], coverCanvas, coverBounds));

      cadenza::MonoFramebuffer previous = host.framebuffer();
      const std::size_t pixelCount =
          static_cast<std::size_t>(previous.width()) * previous.height();
      for (int sample = 1; sample <= 14; ++sample) {
        host.advance(1.0F / 30.0F);
        const std::size_t changed = changedPixels(previous, host.framebuffer());
        CHECK(static_cast<float>(changed) /
                  static_cast<float>(pixelCount) <=
              kMaximumChangedRatio);
        if (sample >= 7) {
          CHECK(sameRect(host.framebuffer(), cover, coverBounds));
        }
        previous = host.framebuffer();
      }
      CHECK_FALSE(host.runtime().transitioning());
      CHECK(host.runtime().currentId() == cadenza::apps::kLauncherAppId);

      const cadenza::MonoFramebuffer stableLauncher = host.framebuffer();
      host.advance(0.0F);
      CHECK(sameFrame(host.framebuffer(), stableLauncher));
    }
  }
}
