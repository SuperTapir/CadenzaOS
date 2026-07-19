#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

#include "cadenza/desktop/png_writer.h"
#include "cadenza/host/headless_host.h"

namespace {

struct GallerySnapshot {
  cadenza::FramebufferProfile profile;
  std::int16_t page;
  std::uint64_t expected;
};

std::uint64_t capture(cadenza::FramebufferProfile profile,
                      std::int16_t page, std::uint64_t expected) {
  cadenza::host::HeadlessHost host{profile};
  REQUIRE(host.runtime().open(cadenza::apps::kGalleryAppId));
  for (int frame = 0; frame < 64 && host.runtime().transitioning(); ++frame) {
    host.step();
  }
  REQUIRE_FALSE(host.runtime().transitioning());
  if (page != 0) {
    cadenza::InputFrame navigate;
    navigate.turn = page;
    host.step(navigate);
  }
  cadenza::InputFrame pause;
  pause.clicked = true;
  host.step(pause);
  cadenza::InputFrame midpoint;
  midpoint.turn = 10;
  host.step(midpoint);
  const std::uint64_t actual = host.framebufferHash();
  const auto directory = std::filesystem::current_path() / "snapshot-failures";
  const auto path = directory /
      ("gallery-p" + std::to_string(static_cast<int>(profile)) + "-page" +
       std::to_string(page) + ".png");
  if (actual != expected) {
    std::filesystem::create_directories(directory);
    CHECK(cadenza::desktop::writePng(path.string(), host.framebuffer()));
  } else {
    std::filesystem::remove(path);
  }
  return actual;
}

}  // namespace

TEST_CASE("approved Gallery midpoint snapshots cover representative effects") {
  const std::array<GallerySnapshot, 12> cases{{
      {cadenza::FramebufferProfile::TEmbed, 0, 7218233691746110281ULL},
      {cadenza::FramebufferProfile::TEmbed, 3, 52789196828127763ULL},
      {cadenza::FramebufferProfile::TEmbed, 8, 17454494746855855220ULL},
      {cadenza::FramebufferProfile::TEmbed, 11, 16647341090174199224ULL},
      {cadenza::FramebufferProfile::TEmbed, 12, 2103464674736729981ULL},
      {cadenza::FramebufferProfile::TEmbed, 13, 5737315058911570614ULL},
      {cadenza::FramebufferProfile::Sharp, 0, 8659909150425046677ULL},
      {cadenza::FramebufferProfile::Sharp, 3, 8975635946972778472ULL},
      {cadenza::FramebufferProfile::Sharp, 8, 6245108958511167040ULL},
      {cadenza::FramebufferProfile::Sharp, 11, 11560105373010971515ULL},
      {cadenza::FramebufferProfile::Sharp, 12, 5121065697262548578ULL},
      {cadenza::FramebufferProfile::Sharp, 13, 17498389982264523906ULL},
  }};
  for (const auto& snapshot : cases) {
    CAPTURE(static_cast<int>(snapshot.profile));
    CAPTURE(snapshot.page);
    CHECK(capture(snapshot.profile, snapshot.page, snapshot.expected) ==
          snapshot.expected);
  }
}
