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
  if (app != cadenza::AppId::Launcher) {
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
       std::to_string(static_cast<int>(app)) + ".png");
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
      {cadenza::FramebufferProfile::TEmbed, cadenza::AppId::Launcher,
       2968281691757874956ULL},
      {cadenza::FramebufferProfile::TEmbed, cadenza::AppId::Clock,
       2172376209712558838ULL},
      {cadenza::FramebufferProfile::TEmbed, cadenza::AppId::Motion,
       11046562126395087774ULL},
      {cadenza::FramebufferProfile::TEmbed, cadenza::AppId::Settings,
       16015422347803220551ULL},
      {cadenza::FramebufferProfile::TEmbed, cadenza::AppId::Gallery,
       14139291840108583961ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::AppId::Launcher,
       5421283046709258962ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::AppId::Clock,
       8667913246713477979ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::AppId::Motion,
       2956592992690758759ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::AppId::Settings,
       16871251489666615369ULL},
      {cadenza::FramebufferProfile::Sharp, cadenza::AppId::Gallery,
       13234575752027769465ULL},
  }};

  for (const SnapshotCase& snapshot : cases) {
    CAPTURE(static_cast<int>(snapshot.profile));
    CAPTURE(static_cast<int>(snapshot.app));
    CHECK(capture(snapshot.profile, snapshot.app, snapshot.expected) ==
          snapshot.expected);
  }
}
