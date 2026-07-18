#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <string>

#include "cadenza/animation/frame_animation.h"
#include "cadenza/animation/state_machine.h"

namespace {

class StateSink final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    last = event;
    ++count;
  }
  cadenza::DiagnosticEvent last{};
  int count = 0;
};

constexpr std::uint8_t kPixels[] = {0xF0};

}  // namespace

TEST_CASE("explicit trigger changes named animation state and restarts target") {
  cadenza::SpriteAtlas<4> atlas{{kPixels, 8, 1, 1}};
  REQUIRE(atlas.add("idle-0", {0, 0, 2, 1}));
  REQUIRE(atlas.add("idle-1", {2, 0, 2, 1}));
  REQUIRE(atlas.add("run-0", {4, 0, 2, 1}));
  REQUIRE(atlas.add("run-1", {6, 0, 2, 1}));
  cadenza::FrameAnimation idle{atlas, 0, 2, 0.1F};
  cadenza::FrameAnimation run{atlas, 2, 2, 0.1F};
  cadenza::AnimationStateMachine<2, 2> machine;
  REQUIRE(machine.addState("idle", idle));
  REQUIRE(machine.addState("run", run));
  REQUIRE(machine.addTransition("idle", "activate", "run",
      cadenza::AnimationResetPolicy::Restart));
  REQUIRE(machine.addTransition("run", "stop", "idle",
      cadenza::AnimationResetPolicy::Preserve));

  REQUIRE(machine.start("idle"));
  idle.update(0.1F);
  run.update(0.1F);
  CHECK(machine.trigger("activate") ==
        cadenza::AnimationTransitionResult::Changed);
  CHECK(std::string(machine.currentState()) == "run");
  CHECK(machine.animation()->frameIndex() == 2);
  machine.update(0.1F);
  CHECK(machine.animation()->frameIndex() == 3);
  CHECK(machine.trigger("stop") ==
        cadenza::AnimationTransitionResult::Changed);
  CHECK(idle.frameIndex() == 1);
}

TEST_CASE("state machine reports no active state and unmatched triggers") {
  cadenza::SpriteAtlas<1> atlas{{kPixels, 8, 1, 1}};
  REQUIRE(atlas.add("only", {0, 0, 1, 1}));
  cadenza::FrameAnimation animation{atlas, 0, 1, 0.1F};
  cadenza::AnimationStateMachine<1, 1> machine;
  REQUIRE(machine.addState("idle", animation));
  CHECK(machine.trigger("go") ==
        cadenza::AnimationTransitionResult::NotStarted);
  REQUIRE(machine.start("idle"));
  CHECK(machine.trigger("missing") ==
        cadenza::AnimationTransitionResult::NoMatch);
  CHECK_FALSE(machine.start("missing"));
}

TEST_CASE("invalid and overflowing tables are rejected with diagnostics") {
  StateSink diagnostics;
  cadenza::SpriteAtlas<1> atlas{{kPixels, 8, 1, 1}};
  REQUIRE(atlas.add("only", {0, 0, 1, 1}));
  cadenza::FrameAnimation animation{atlas, 0, 1, 0.1F};
  cadenza::AnimationStateMachine<1, 1> machine{&diagnostics};
  REQUIRE(machine.addState("idle", animation));
  CHECK_FALSE(machine.addState("idle", animation));
  CHECK(diagnostics.last.code == cadenza::DiagnosticCode::InvalidOperation);
  CHECK_FALSE(machine.addState("overflow", animation));
  CHECK(diagnostics.last.code == cadenza::DiagnosticCode::CapacityExceeded);
  CHECK_FALSE(machine.addTransition("idle", "go", "missing"));
  CHECK(diagnostics.last.code == cadenza::DiagnosticCode::InvalidOperation);
}
