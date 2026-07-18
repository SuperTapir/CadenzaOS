#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "cadenza/desktop/desktop_model.h"
#include "cadenza/host/headless_host.h"
#include "cadenza/core/sprite_atlas.h"

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

TEST_CASE("desktop display palette matches reflective reference and keeps pure mode") {
  using cadenza::desktop::DisplayPalette;
  DisplayPalette palette = DisplayPalette::Pure;
  CHECK(cadenza::desktop::parseDisplayPalette(palette, "reflective"));
  CHECK(palette == DisplayPalette::Reflective);
  const auto reflectiveInk =
      cadenza::desktop::displayColor(true, palette);
  const auto reflectivePaper =
      cadenza::desktop::displayColor(false, palette);
  CHECK(reflectiveInk.r == 50);
  CHECK(reflectiveInk.g == 47);
  CHECK(reflectiveInk.b == 40);
  CHECK(reflectiveInk.a == 255);
  CHECK(reflectivePaper.r == 177);
  CHECK(reflectivePaper.g == 174);
  CHECK(reflectivePaper.b == 167);
  CHECK(reflectivePaper.a == 255);

  CHECK(cadenza::desktop::parseDisplayPalette(palette, "pure"));
  const auto pureInk = cadenza::desktop::displayColor(true, palette);
  const auto purePaper = cadenza::desktop::displayColor(false, palette);
  CHECK(pureInk.r == 0);
  CHECK(pureInk.g == 0);
  CHECK(pureInk.b == 0);
  CHECK(purePaper.r == 255);
  CHECK(purePaper.g == 255);
  CHECK(purePaper.b == 255);

  CHECK_FALSE(cadenza::desktop::parseDisplayPalette(palette, "sepia"));
  CHECK(palette == DisplayPalette::Pure);
  CHECK_FALSE(cadenza::desktop::parseDisplayPalette(palette, nullptr));
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
  overlay.update(59.9F, 16.7F, cadenza::apps::kMotionAppId, {2, false, false,
                                                       true, false, 0});
  overlay.updateResources(6800, 12000, 3, 123456);
  CHECK(overlay.visible);
  CHECK(overlay.app == cadenza::apps::kMotionAppId);
  CHECK(overlay.framebufferBytes == 6800);
  CHECK(overlay.framebufferCapacity == 12000);
  CHECK(overlay.capacityOverflows == 3);
  CHECK(overlay.heapEstimateBytes == 123456);
  CHECK(cadenza::host::framebufferHash(framebuffer) == before);
}

TEST_CASE("optional device preview preserves hash and converts scaled coordinates") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  framebuffer.setPixel(9, 7);
  const auto before = cadenza::host::framebufferHash(framebuffer);
  const auto plain = cadenza::desktop::DevicePreviewLayout::make(320, 170, 3, false);
  const auto framed = cadenza::desktop::DevicePreviewLayout::make(320, 170, 3, true);
  CHECK(plain.windowWidth == 960);
  CHECK(framed.windowWidth > plain.windowWidth);
  std::int32_t x = -1;
  std::int32_t y = -1;
  REQUIRE(framed.toFramebuffer((framed.screenX + 9) * 3,
                               (framed.screenY + 7) * 3, x, y));
  CHECK(x == 9);
  CHECK(y == 7);
  CHECK_FALSE(framed.toFramebuffer(0, 0, x, y));
  CHECK(cadenza::host::framebufferHash(framebuffer) == before);
}

TEST_CASE("simulator diagnostics expose canvas and capacity misuse") {
  cadenza::desktop::DesktopDiagnosticLog diagnostics;
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
  canvas.fillRect(0, 0, 0, 2);
  const std::uint8_t pixels[] = {0x80};
  cadenza::SpriteAtlas<0> atlas{{pixels, 1, 1, 1}, &diagnostics};
  CHECK_FALSE(atlas.add("full", {0, 0, 1, 1}));

  CHECK(diagnostics.totalEvents() == 2);
  CHECK(diagnostics.capacityOverflows() == 1);
  REQUIRE(diagnostics.recent(0) != nullptr);
  CHECK(diagnostics.recent(0)->code == cadenza::DiagnosticCode::CapacityExceeded);
  REQUIRE(diagnostics.recent(1) != nullptr);
  CHECK(diagnostics.recent(1)->code == cadenza::DiagnosticCode::InvalidGeometry);
}

TEST_CASE("desktop overlay distinguishes current-frame diagnostics from history") {
  cadenza::desktop::DesktopDiagnosticLog diagnostics;
  diagnostics.emit({cadenza::DiagnosticCategory::Graphics,
                    cadenza::DiagnosticCode::ClippedGeometry,
                    "old clipped geometry", 0});
  REQUIRE(diagnostics.recent(0) != nullptr);
  diagnostics.beginFrame();
  CHECK(diagnostics.recentThisFrame(0) == nullptr);
  CHECK(diagnostics.currentFrameEventCount() == 0);

  diagnostics.emit({cadenza::DiagnosticCategory::Graphics,
                    cadenza::DiagnosticCode::InvalidGeometry,
                    "current invalid geometry", 0});
  REQUIRE(diagnostics.recentThisFrame(0) != nullptr);
  CHECK(diagnostics.recentThisFrame(0)->code ==
        cadenza::DiagnosticCode::InvalidGeometry);
  CHECK(diagnostics.currentFrameEventCount() == 1);
  CHECK(diagnostics.totalEvents() == 2);
}
