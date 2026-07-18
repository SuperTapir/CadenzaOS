#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "cadenza/desktop/desktop_model.h"
#include "cadenza/host/headless_host.h"

TEST_CASE("desktop profile and integer scale accept only supported values") {
  cadenza::desktop::DesktopConfig config;
  CHECK(cadenza::desktop::configure(config, "t-embed", 1));
  CHECK(config.profile == cadenza::FramebufferProfile::TEmbed);
  CHECK(config.width == 320);
  CHECK(config.height == 170);
  CHECK(cadenza::desktop::configure(config, "sharp", 4));
  CHECK(config.profile == cadenza::FramebufferProfile::Sharp);
  CHECK(config.width == 400);
  CHECK(config.height == 240);
  CHECK_FALSE(cadenza::desktop::configure(config, "unknown", 2));
  CHECK_FALSE(cadenza::desktop::configure(config, "sharp", 0));
  CHECK_FALSE(cadenza::desktop::configure(config, "sharp", 5));
}

TEST_CASE("desktop mapper emits raw turn and aggregate button semantics") {
  cadenza::desktop::DesktopInputMapper mapper;
  CHECK(mapper.wheel(0.25F, 10));
  CHECK(mapper.key(cadenza::desktop::DesktopKey::Left, true, false, 11));
  CHECK(mapper.key(cadenza::desktop::DesktopKey::Right, true, true, 12));
  CHECK(mapper.key(cadenza::desktop::DesktopKey::Space, true, false, 13));
  CHECK_FALSE(mapper.key(cadenza::desktop::DesktopKey::Enter, true, true, 14));
  CHECK(mapper.key(cadenza::desktop::DesktopKey::Enter, true, false, 15));
  CHECK(mapper.key(cadenza::desktop::DesktopKey::Space, false, false, 16));
  CHECK(mapper.key(cadenza::desktop::DesktopKey::Enter, false, false, 17));

  cadenza::RawInputEvent event;
  REQUIRE(mapper.poll(event));
  CHECK(event.type == cadenza::RawInputType::Turn);
  CHECK(event.value == 1);
  REQUIRE(mapper.poll(event));
  CHECK(event.value == -1);
  REQUIRE(mapper.poll(event));
  CHECK(event.value == 1);
  REQUIRE(mapper.poll(event));
  CHECK(event.type == cadenza::RawInputType::ButtonDown);
  REQUIRE(mapper.poll(event));
  CHECK(event.type == cadenza::RawInputType::ButtonUp);
  CHECK_FALSE(mapper.poll(event));
}

TEST_CASE("overlay metadata never changes canonical framebuffer truth") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  framebuffer.setPixel(3, 4);
  const auto before = cadenza::host::framebufferHash(framebuffer);
  cadenza::desktop::OverlayState overlay;
  overlay.toggle();
  overlay.update(59.9F, 16.7F, cadenza::AppId::Motion, {2, false, false,
                                                       true, false, 0});
  CHECK(overlay.visible);
  CHECK(overlay.app == cadenza::AppId::Motion);
  CHECK(cadenza::host::framebufferHash(framebuffer) == before);
}
