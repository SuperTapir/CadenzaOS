#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cmath>

#include "cadenza/host/headless_host.h"

namespace {
class TickApp final : public cadenza::App {
 public:
  const char* name() const noexcept override { return "Tick"; }
  void update(cadenza::Seconds dt, const cadenza::InputFrame& input,
              cadenza::AppRuntime&) noexcept override {
    elapsed += dt;
    turn += input.turn;
    ++updates;
  }
  void render(cadenza::MonoCanvas& canvas,
              const cadenza::AppRuntime&) noexcept override {
    canvas.clear(false);
    canvas.pixel(updates, turn + 10);
  }

  float elapsed = 0.0F;
  int turn = 0;
  int updates = 0;
};
}  // namespace

TEST_CASE("deterministic runner advances only by its fixed delta") {
  TickApp app;
  cadenza::AppRuntime runtime;
  REQUIRE(runtime.registerApp(cadenza::AppId::Launcher, app, false));
  REQUIRE(runtime.begin(cadenza::AppId::Launcher));
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  cadenza::host::DeterministicRunner runner{runtime, canvas, framebuffer,
                                             1.0F / 60.0F};

  runner.step();
  runner.step();
  runner.step();
  CHECK(runner.frameIndex() == 3);
  CHECK(std::abs(runner.simulationSeconds() - 0.05F) < 0.00001F);
  CHECK(std::abs(app.elapsed - 0.05F) < 0.00001F);
}

TEST_CASE("scripted input is delivered on exact simulation frames") {
  TickApp app;
  cadenza::AppRuntime runtime;
  REQUIRE(runtime.registerApp(cadenza::AppId::Launcher, app, false));
  REQUIRE(runtime.begin(cadenza::AppId::Launcher));
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  cadenza::host::DeterministicRunner runner{runtime, canvas, framebuffer,
                                             0.01F};
  cadenza::host::InputScript<3> script;
  cadenza::InputFrame first;
  first.turn = 2;
  cadenza::InputFrame third;
  third.turn = -1;
  REQUIRE(script.add(0, first));
  REQUIRE(script.add(2, third));

  runner.runFrames(4, script);
  CHECK(app.updates == 4);
  CHECK(app.turn == 1);
  CHECK(runner.frameIndex() == 4);
}

TEST_CASE("fresh HeadlessHost replays produce equal per-frame hashes") {
  cadenza::host::HeadlessHost first{cadenza::FramebufferProfile::TEmbed};
  cadenza::host::HeadlessHost second{cadenza::FramebufferProfile::TEmbed};
  cadenza::host::InputScript<2> script;
  cadenza::InputFrame click;
  click.clicked = true;
  REQUIRE(script.add(1, click));

  for (cadenza::FrameIndex frame = 0; frame < 30; ++frame) {
    first.step(script.inputAt(frame));
    second.step(script.inputAt(frame));
    CHECK(first.framebufferHash() == second.framebufferHash());
  }
  CHECK(first.runtime().currentId() == second.runtime().currentId());
}

TEST_CASE("framebuffer hash is stable and pixel-sensitive at both profiles") {
  for (const cadenza::FramebufferProfile profile : {
           cadenza::FramebufferProfile::TEmbed,
           cadenza::FramebufferProfile::Sharp}) {
    cadenza::MonoFramebuffer first{profile};
    cadenza::MonoFramebuffer second{profile};
    CHECK(cadenza::host::framebufferHash(first) ==
          cadenza::host::framebufferHash(second));
    second.setPixel(second.width() - 1, second.height() - 1);
    CHECK(cadenza::host::framebufferHash(first) !=
          cadenza::host::framebufferHash(second));
  }
}
