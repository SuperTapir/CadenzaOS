#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <string>
#include <vector>

#include "cadenza/core/app_runtime.h"

namespace {
class FakeApp final : public cadenza::App {
 public:
  FakeApp(const char* appName, std::vector<std::string>& events)
      : appName_(appName), events_(events) {}

  const char* name() const noexcept override { return appName_; }
  void onEnter() noexcept override { events_.push_back(std::string(appName_) + ":enter"); }
  void onExit() noexcept override { events_.push_back(std::string(appName_) + ":exit"); }
  void update(cadenza::Seconds, const cadenza::InputFrame&,
              cadenza::AppRuntime&) noexcept override {
    ++updates;
    events_.push_back(std::string(appName_) + ":update");
  }
  void render(cadenza::MonoCanvas&, const cadenza::AppRuntime&) noexcept override {
    ++renders;
  }

  int updates = 0;
  int renders = 0;

 private:
  const char* appName_;
  std::vector<std::string>& events_;
};

struct Fixture {
  std::vector<std::string> events;
  FakeApp launcher{"Launcher", events};
  FakeApp clock{"Clock", events};
  FakeApp motion{"Motion", events};
  cadenza::AppRuntime runtime;

  Fixture() {
    REQUIRE(runtime.registerApp(cadenza::AppId::Launcher, launcher, false));
    REQUIRE(runtime.registerApp(cadenza::AppId::Clock, clock));
    REQUIRE(runtime.registerApp(cadenza::AppId::Motion, motion));
  }
};
}  // namespace

TEST_CASE("runtime registry preserves static IDs visibility and names") {
  Fixture fixture;
  CHECK(fixture.runtime.launcherAppCount() == 2);
  CHECK(fixture.runtime.launcherAppAt(0) == cadenza::AppId::Clock);
  CHECK(fixture.runtime.launcherAppAt(1) == cadenza::AppId::Motion);
  CHECK(fixture.runtime.launcherAppAt(99) == cadenza::AppId::Launcher);
  CHECK(std::string(fixture.runtime.appName(cadenza::AppId::Motion)) == "Motion");
  CHECK(std::string(fixture.runtime.appName(cadenza::AppId::Settings)) == "MISSING");
  CHECK_FALSE(fixture.runtime.registerApp(cadenza::AppId::Count, fixture.clock));
}

TEST_CASE("begin enters one valid initial App exactly once") {
  Fixture fixture;
  CHECK_FALSE(fixture.runtime.begin(cadenza::AppId::Settings));
  CHECK(fixture.events.empty());
  REQUIRE(fixture.runtime.begin(cadenza::AppId::Launcher));
  CHECK(fixture.events == std::vector<std::string>{"Launcher:enter"});
  CHECK_FALSE(fixture.runtime.begin(cadenza::AppId::Clock));
  CHECK(fixture.runtime.currentId() == cadenza::AppId::Launcher);
}

TEST_CASE("open guards current missing invalid and in-flight destinations") {
  Fixture fixture;
  REQUIRE(fixture.runtime.begin(cadenza::AppId::Launcher));
  CHECK_FALSE(fixture.runtime.open(cadenza::AppId::Launcher));
  CHECK_FALSE(fixture.runtime.open(cadenza::AppId::Settings));
  CHECK_FALSE(fixture.runtime.open(cadenza::AppId::Count));
  REQUIRE(fixture.runtime.open(cadenza::AppId::Clock));
  CHECK(fixture.runtime.transitioning());
  CHECK_FALSE(fixture.runtime.open(cadenza::AppId::Motion));
}

TEST_CASE("transition orders exit before enter at the midpoint") {
  Fixture fixture;
  REQUIRE(fixture.runtime.begin(cadenza::AppId::Launcher));
  REQUIRE(fixture.runtime.open(cadenza::AppId::Clock));
  fixture.runtime.update(0.15F, {});
  CHECK(fixture.events == std::vector<std::string>{"Launcher:enter"});
  fixture.runtime.update(0.02F, {});
  CHECK(fixture.events ==
        std::vector<std::string>{"Launcher:enter", "Launcher:exit", "Clock:enter"});
  CHECK(fixture.runtime.currentId() == cadenza::AppId::Clock);
}

TEST_CASE("only active App updates and transition input is frozen") {
  Fixture fixture;
  REQUIRE(fixture.runtime.begin(cadenza::AppId::Launcher));
  cadenza::InputFrame input;
  input.turn = 3;
  input.clicked = true;
  fixture.runtime.update(0.01F, input);
  CHECK(fixture.launcher.updates == 1);
  REQUIRE(fixture.runtime.open(cadenza::AppId::Clock));
  input.turn = 7;
  fixture.runtime.update(0.10F, input);
  fixture.runtime.update(0.23F, input);
  CHECK_FALSE(fixture.runtime.transitioning());
  CHECK(fixture.launcher.updates == 1);
  CHECK(fixture.clock.updates == 0);
  fixture.runtime.update(0.01F, {});
  CHECK(fixture.clock.updates == 1);
}

TEST_CASE("long press starts system return without updating active App") {
  Fixture fixture;
  REQUIRE(fixture.runtime.begin(cadenza::AppId::Clock));
  cadenza::InputFrame input;
  input.longPressed = true;
  fixture.runtime.update(0.01F, input);
  CHECK(fixture.runtime.transitioning());
  CHECK(fixture.clock.updates == 0);
  fixture.runtime.update(0.17F, {});
  CHECK(fixture.runtime.currentId() == cadenza::AppId::Launcher);
  CHECK(fixture.events ==
        std::vector<std::string>{"Clock:enter", "Clock:exit", "Launcher:enter"});
}
