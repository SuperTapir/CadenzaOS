#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstdint>

#include "cadenza/host/headless_host.h"

namespace {

struct GallerySnapshot {
  cadenza::FramebufferProfile profile;
  std::int16_t page;
  std::uint64_t expected;
};

std::uint64_t capture(cadenza::FramebufferProfile profile,
                      std::int16_t page) {
  cadenza::host::HeadlessHost host{profile};
  REQUIRE(host.runtime().open(cadenza::AppId::Gallery));
  for (int frame = 0; frame < 32 && host.runtime().transitioning(); ++frame) {
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
  return host.framebufferHash();
}

}  // namespace

TEST_CASE("approved Gallery midpoint snapshots cover representative effects") {
  const std::array<GallerySnapshot, 12> cases{{
      {cadenza::FramebufferProfile::TEmbed, 0, 11440640267301194983ULL},
      {cadenza::FramebufferProfile::TEmbed, 3, 4137199271609631912ULL},
      {cadenza::FramebufferProfile::TEmbed, 8, 9878436293935264858ULL},
      {cadenza::FramebufferProfile::TEmbed, 11, 11948840630181533057ULL},
      {cadenza::FramebufferProfile::TEmbed, 12, 9026563825525625221ULL},
      {cadenza::FramebufferProfile::TEmbed, 13, 13029273646656056373ULL},
      {cadenza::FramebufferProfile::Sharp, 0, 17615136595640686815ULL},
      {cadenza::FramebufferProfile::Sharp, 3, 15534794435185275820ULL},
      {cadenza::FramebufferProfile::Sharp, 8, 13438891652020997183ULL},
      {cadenza::FramebufferProfile::Sharp, 11, 7317937573070898047ULL},
      {cadenza::FramebufferProfile::Sharp, 12, 1640077496920006473ULL},
      {cadenza::FramebufferProfile::Sharp, 13, 17314096927354342150ULL},
  }};
  for (const auto& snapshot : cases) {
    CAPTURE(static_cast<int>(snapshot.profile));
    CAPTURE(snapshot.page);
    CHECK(capture(snapshot.profile, snapshot.page) == snapshot.expected);
  }
}
