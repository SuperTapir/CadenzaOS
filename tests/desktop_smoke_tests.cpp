#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "cadenza/core/input_adapter.h"
#include "cadenza/desktop/desktop_model.h"
#include "cadenza/host/headless_host.h"

namespace {
class SmokeHarness final : public cadenza::MonotonicClock {
 public:
  explicit SmokeHarness(cadenza::DiagnosticSink* diagnostics = nullptr)
      : host(cadenza::FramebufferProfile::TEmbed, 1.0F / 60.0F,
             diagnostics) {}

  cadenza::MonotonicMillis nowMs() const noexcept override { return now_; }

  void turnRight() {
    REQUIRE(mapper_.key(cadenza::desktop::DesktopKey::Right, true, false,
                        now_));
    deliver();
  }

  void turnRight(int count) {
    for (int index = 0; index < count; ++index) turnRight();
  }

  void tapEnter() {
    REQUIRE(mapper_.key(cadenza::desktop::DesktopKey::Enter, true, false,
                        now_));
    now_ += 30;
    deliver();
    now_ += 10;
    REQUIRE(mapper_.key(cadenza::desktop::DesktopKey::Enter, false, false,
                        now_));
    now_ += 30;
    deliver();
    settle();
  }

  void returnHomeThroughSystemMenu() {
    REQUIRE(mapper_.key(cadenza::desktop::DesktopKey::Space, true, false,
                        now_));
    now_ += 700;
    deliver();
    now_ += 10;
    REQUIRE(mapper_.key(cadenza::desktop::DesktopKey::Space, false, false,
                        now_));
    now_ += 30;
    deliver();
    REQUIRE(host.runtime().systemMenuActive());
    turnRight();
    tapEnter();
    settle();
  }

  void openAndResumeSystemMenu() {
    REQUIRE(mapper_.key(cadenza::desktop::DesktopKey::Space, true, false,
                        now_));
    now_ += 700;
    deliver();
    now_ += 10;
    REQUIRE(mapper_.key(cadenza::desktop::DesktopKey::Space, false, false,
                        now_));
    now_ += 30;
    deliver();
    REQUIRE(host.runtime().systemMenuActive());
    tapEnter();
    REQUIRE_FALSE(host.runtime().systemMenuActive());
  }

  void advanceFrames(int count) {
    for (int frame = 0; frame < count; ++frame) {
      now_ += 17;
      deliver();
    }
  }

  cadenza::host::HeadlessHost host;

 private:
  void deliver() {
    cadenza::pumpInput(mapper_, *this, reducer_);
    host.step(reducer_.takeFrame());
  }

  void settle() {
    for (int frame = 0; frame < 32 && host.runtime().transitioning(); ++frame) {
      host.step();
    }
    REQUIRE_FALSE(host.runtime().transitioning());
  }

  cadenza::desktop::DesktopInputMapper mapper_;
  cadenza::InputReducer reducer_;
  cadenza::MonotonicMillis now_ = 0;
};
}  // namespace

TEST_CASE("desktop input path visits every bundled App and returns home") {
  SmokeHarness harness;
  CHECK(harness.host.runtime().currentId() == cadenza::apps::kLauncherAppId);

  harness.tapEnter();
  CHECK(harness.host.runtime().currentId() == cadenza::apps::kClockAppId);
  harness.returnHomeThroughSystemMenu();
  CHECK(harness.host.runtime().currentId() == cadenza::apps::kLauncherAppId);

  harness.turnRight();
  harness.tapEnter();
  CHECK(harness.host.runtime().currentId() == cadenza::apps::kMotionAppId);
  harness.returnHomeThroughSystemMenu();
  CHECK(harness.host.runtime().currentId() == cadenza::apps::kLauncherAppId);

  harness.turnRight();
  harness.tapEnter();
  CHECK(harness.host.runtime().currentId() == cadenza::apps::kSettingsAppId);
  harness.returnHomeThroughSystemMenu();
  CHECK(harness.host.runtime().currentId() == cadenza::apps::kLauncherAppId);

  harness.turnRight();
  harness.tapEnter();
  CHECK(harness.host.runtime().currentId() == cadenza::apps::kGalleryAppId);
  const std::uint64_t galleryStart = harness.host.framebufferHash();
  harness.turnRight();
  harness.tapEnter();
  harness.turnRight();
  CHECK(harness.host.framebufferHash() != galleryStart);
  harness.returnHomeThroughSystemMenu();
  CHECK(harness.host.runtime().currentId() == cadenza::apps::kLauncherAppId);
}

TEST_CASE("desktop Settings changes Launcher axis and moving selection opens") {
  cadenza::desktop::DesktopDiagnosticLog diagnostics;
  SmokeHarness harness{&diagnostics};

  harness.turnRight(2);
  harness.tapEnter();
  REQUIRE(harness.host.runtime().currentId() ==
          cadenza::apps::kSettingsAppId);
  harness.turnRight(4);
  harness.tapEnter();
  CHECK(harness.host.services().snapshot().launcherOrientation ==
        cadenza::LauncherOrientation::Horizontal);

  harness.returnHomeThroughSystemMenu();
  REQUIRE(harness.host.runtime().currentId() ==
          cadenza::apps::kLauncherAppId);
  harness.advanceFrames(120);
  const std::uint64_t stable = harness.host.framebufferHash();
  harness.openAndResumeSystemMenu();
  harness.advanceFrames(120);
  CHECK(harness.host.framebufferHash() == stable);

  diagnostics.beginFrame();
  harness.advanceFrames(1);
  CHECK(diagnostics.currentFrameEventCount() == 0);

  harness.turnRight();
  harness.tapEnter();
  CHECK(harness.host.runtime().currentId() ==
        cadenza::apps::kGalleryAppId);
}
