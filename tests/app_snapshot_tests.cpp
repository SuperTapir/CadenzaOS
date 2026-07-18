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

std::uint64_t capture(cadenza::FramebufferProfile profile,
                      cadenza::AppId app, std::uint64_t expected) {
  cadenza::host::HeadlessHost host{profile};
  if (app != cadenza::apps::kLauncherAppId) {
    REQUIRE(host.runtime().open(app));
    for (int frame = 0; frame < 32 && host.runtime().transitioning(); ++frame) {
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
