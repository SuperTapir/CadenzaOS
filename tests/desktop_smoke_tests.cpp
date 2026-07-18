#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "cadenza/core/input_adapter.h"
#include "cadenza/desktop/desktop_model.h"
#include "cadenza/host/headless_host.h"

namespace {
class SmokeHarness final : public cadenza::MonotonicClock {
 public:
  cadenza::MonotonicMillis nowMs() const noexcept override { return now_; }

  void turnRight() {
    REQUIRE(mapper_.key(cadenza::desktop::DesktopKey::Right, true, false,
                        now_));
    deliver();
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

  void holdSpaceHome() {
    REQUIRE(mapper_.key(cadenza::desktop::DesktopKey::Space, true, false,
                        now_));
    now_ += 700;
    deliver();
    now_ += 10;
    REQUIRE(mapper_.key(cadenza::desktop::DesktopKey::Space, false, false,
                        now_));
    now_ += 30;
    deliver();
    settle();
  }

  cadenza::host::HeadlessHost host{cadenza::FramebufferProfile::TEmbed};

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
  CHECK(harness.host.runtime().currentId() == cadenza::AppId::Launcher);

  harness.tapEnter();
  CHECK(harness.host.runtime().currentId() == cadenza::AppId::Clock);
  harness.holdSpaceHome();
  CHECK(harness.host.runtime().currentId() == cadenza::AppId::Launcher);

  harness.turnRight();
  harness.tapEnter();
  CHECK(harness.host.runtime().currentId() == cadenza::AppId::Motion);
  harness.holdSpaceHome();
  CHECK(harness.host.runtime().currentId() == cadenza::AppId::Launcher);

  harness.turnRight();
  harness.tapEnter();
  CHECK(harness.host.runtime().currentId() == cadenza::AppId::Settings);
  harness.holdSpaceHome();
  CHECK(harness.host.runtime().currentId() == cadenza::AppId::Launcher);

  harness.turnRight();
  harness.tapEnter();
  CHECK(harness.host.runtime().currentId() == cadenza::AppId::Gallery);
  const std::uint64_t galleryStart = harness.host.framebufferHash();
  harness.turnRight();
  harness.tapEnter();
  harness.turnRight();
  CHECK(harness.host.framebufferHash() != galleryStart);
  harness.holdSpaceHome();
  CHECK(harness.host.runtime().currentId() == cadenza::AppId::Launcher);
}
