#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstddef>
#include <cstdlib>
#include <new>

#include "cadenza/apps/apps.h"
#include "cadenza/core/app_runtime.h"
#include "cadenza/core/mono_canvas.h"
#include "cadenza/system/system_service_host.h"
#include "cadenza/system/frame_coordinator.h"

namespace {
std::size_t gAllocationCount = 0;
}

void* operator new(std::size_t size) {
  if (void* memory = std::malloc(size)) {
    ++gAllocationCount;
    return memory;
  }
  throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
  if (void* memory = std::malloc(size)) {
    ++gAllocationCount;
    return memory;
  }
  throw std::bad_alloc{};
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept {
  std::free(memory);
}

namespace {
void frame(cadenza::AppRuntime& runtime,
           cadenza::system::SystemServiceHost& services,
           cadenza::MonoCanvas& canvas,
           const cadenza::InputFrame& input = {}) noexcept {
  const auto& snapshot = services.beginFrame(1.0F / 60.0F);
  runtime.updateWithSystem(1.0F / 60.0F, input, snapshot, services);
  runtime.renderWithSystem(canvas, snapshot);
  services.commitCommands();
}

void frames(cadenza::AppRuntime& runtime,
            cadenza::system::SystemServiceHost& services,
            cadenza::MonoCanvas& canvas, int count) noexcept {
  for (int index = 0; index < count; ++index) {
    frame(runtime, services, canvas);
  }
}
}  // namespace

TEST_CASE("App handoff and warped Menu perform no runtime allocations") {
  for (const auto profile : {cadenza::FramebufferProfile::TEmbed,
                             cadenza::FramebufferProfile::Sharp}) {
    cadenza::LauncherApp launcher;
    cadenza::TimerApp timer;
    cadenza::system::SystemServiceHost services;
    cadenza::AppRuntime runtime{profile};
    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer};

    REQUIRE(runtime.registerApp(cadenza::apps::kLauncherAppId, launcher,
                                false));
    REQUIRE(runtime.registerApp(cadenza::apps::kTimerAppId, timer));
    REQUIRE(runtime.configureHome(cadenza::apps::kLauncherAppId));
    REQUIRE(runtime.begin(cadenza::apps::kLauncherAppId));
    runtime.bindSystem(services.snapshot(), services);
    runtime.renderWithSystem(canvas, services.snapshot());

    const std::size_t before = gAllocationCount;
    bool accepted = runtime.open(cadenza::apps::kTimerAppId);
    frames(runtime, services, canvas, 50);

    cadenza::InputFrame toggle;
    toggle.clicked = true;
    frame(runtime, services, canvas, toggle);
    frames(runtime, services, canvas, 15);
    frame(runtime, services, canvas, toggle);
    frames(runtime, services, canvas, 12);
    frame(runtime, services, canvas, toggle);
    frames(runtime, services, canvas, 12);

    cadenza::InputFrame held;
    held.longPressed = true;
    held.heldMs = 650;
    frame(runtime, services, canvas, held);
    cadenza::InputFrame released;
    released.released = true;
    frame(runtime, services, canvas, released);
    frames(runtime, services, canvas, 12);
    cadenza::InputFrame resume;
    resume.clicked = true;
    frame(runtime, services, canvas, resume);
    frames(runtime, services, canvas, 12);

    accepted = accepted && runtime.open(cadenza::apps::kLauncherAppId);
    frames(runtime, services, canvas, 28);
    const std::size_t after = gAllocationCount;

    CHECK(accepted);
    CHECK_FALSE(runtime.transitioning());
    CHECK_FALSE(runtime.systemMenuActive());
    CHECK(runtime.currentId() == cadenza::apps::kLauncherAppId);
    CHECK(after == before);
  }
}

TEST_CASE("Timer critical alert and acknowledgement allocate no memory") {
  for (const auto profile : {cadenza::FramebufferProfile::TEmbed,
                             cadenza::FramebufferProfile::Sharp}) {
    cadenza::LauncherApp launcher;
    cadenza::TimerApp timer;
    cadenza::system::SystemServiceHost services;
    cadenza::AppRuntime runtime{profile, cadenza::kCutTransition};
    cadenza::MonoFramebuffer framebuffer{profile};
    cadenza::MonoCanvas canvas{framebuffer};
    REQUIRE(runtime.registerApp(cadenza::apps::kLauncherAppId, launcher,
                                false));
    REQUIRE(runtime.registerApp(
        cadenza::apps::kTimerAppId, timer, true,
        cadenza::apps::builtinAppCapabilities(cadenza::apps::kTimerAppId)));
    REQUIRE(runtime.configureHome(cadenza::apps::kLauncherAppId));
    REQUIRE(runtime.begin(cadenza::apps::kTimerAppId));

    cadenza::system::FrameCoordinator::runFrameAt(
        services, runtime, canvas, 1000, 0.0F, {});
    REQUIRE(services.submit(
        {cadenza::ResourceOwner::app(cadenza::apps::kTimerAppId),
         cadenza::SystemCommand::startTimer(60000), 0}));
    services.commitCommands();
    const std::size_t before = gAllocationCount;

    cadenza::InputFrame held;
    held.heldMs = 700;
    cadenza::system::FrameCoordinator::runFrameAt(
        services, runtime, canvas, 61000, 0.0F, held);
    cadenza::InputFrame staleRelease;
    staleRelease.released = true;
    staleRelease.clicked = true;
    cadenza::system::FrameCoordinator::runFrameAt(
        services, runtime, canvas, 61001, 0.0F, staleRelease);
    cadenza::system::FrameCoordinator::runFrameAt(
        services, runtime, canvas, 61002, 0.0F, staleRelease);
    cadenza::system::FrameCoordinator::runFrameAt(
        services, runtime, canvas, 61003, 0.0F, {});

    CHECK(services.snapshot().timer.state == cadenza::TimerState::Ready);
    CHECK_FALSE(runtime.systemSurfaces().timerAlertActive());
    CHECK(gAllocationCount == before);
  }
}
