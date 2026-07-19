#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>

#include "cadenza/core/app_runtime.h"
#include "cadenza/core/transition.h"

namespace {
constexpr cadenza::AppId kHomeAppId{1};
constexpr cadenza::AppId kTimerAppId{2};

using BufferBytes =
    std::array<std::uint8_t, cadenza::MonoFramebuffer::kCapacityBytes>;

BufferBytes snapshot(const cadenza::MonoFramebuffer& framebuffer) {
  BufferBytes result{};
  std::copy_n(framebuffer.data(), framebuffer.sizeBytes(), result.data());
  return result;
}

bool equals(const cadenza::MonoFramebuffer& framebuffer,
            const BufferBytes& expected) {
  return std::equal(framebuffer.data(),
                    framebuffer.data() + framebuffer.sizeBytes(),
                    expected.data());
}

class PatternApp final : public cadenza::App {
 public:
  PatternApp(const char* appName, int offset) : appName_(appName), offset_(offset) {}

  const char* name() const noexcept override { return appName_; }
  void update(const cadenza::AppUpdateContext&) noexcept override {}
  void render(cadenza::MonoCanvas& canvas,
              const cadenza::AppRenderContext&) noexcept override {
    canvas.clear(false);
    canvas.pixel(offset_, 1);
    canvas.pixel(offset_ + 2, 2);
  }

 private:
  const char* appName_;
  int offset_;
};

class LaunchProbeApp final : public cadenza::App {
 public:
  explicit LaunchProbeApp(const char* value, bool hasLaunch = false)
      : value_(value), hasLaunch_(hasLaunch) {}
  const char* name() const noexcept override { return value_; }
  void onEnter() noexcept override { ++enters; }
  void onExit() noexcept override { ++exits; }
  void update(const cadenza::AppUpdateContext&) noexcept override {
    ++updates;
  }
  void render(cadenza::MonoCanvas& canvas,
              const cadenza::AppRenderContext&) noexcept override {
    canvas.clear(false);
    canvas.pixel(7, 7, true);
  }
  bool renderLauncherCover(cadenza::MonoCanvas& canvas,
                           cadenza::Rect bounds) const noexcept override {
    canvas.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, false);
    canvas.rect(bounds.x, bounds.y, bounds.width, bounds.height, true);
    return true;
  }
  bool renderLaunchFrame(cadenza::MonoCanvas& canvas,
                         float progress,
                         const cadenza::AppRenderContext&) const noexcept override {
    ++launchCalls;
    lastLaunchProgress = progress;
    if (!hasLaunch_) return false;
    canvas.clear(true);
    canvas.pixel(static_cast<int>(progress * 20.0F), 3, false);
    return true;
  }

  int enters = 0;
  int exits = 0;
  int updates = 0;
  mutable int launchCalls = 0;
  mutable float lastLaunchProgress = -1.0F;

 private:
  const char* value_;
  bool hasLaunch_ = false;
};

class CaptureTransition final : public cadenza::Transition {
 public:
  void compose(const cadenza::MonoFramebuffer& outgoing,
               const cadenza::MonoFramebuffer& incoming,
               cadenza::MonoCanvas& output,
               float progress) const noexcept override {
    outgoingBefore = snapshot(outgoing);
    incomingBefore = snapshot(incoming);
    output.drawFramebuffer(progress < 0.5F ? outgoing : incoming,
                           {0, 0, outgoing.width(), outgoing.height()}, 0, 0);
    sourcesStayedImmutable = equals(outgoing, outgoingBefore) &&
                             equals(incoming, incomingBefore);
    ++calls;
  }

  mutable BufferBytes outgoingBefore{};
  mutable BufferBytes incomingBefore{};
  mutable bool sourcesStayedImmutable = false;
  mutable int calls = 0;
};
}  // namespace

TEST_CASE("runtime captures outgoing at open and incoming after lifecycle swap") {
  CaptureTransition transition;
  cadenza::AppRuntime runtime{cadenza::FramebufferProfile::TEmbed, transition};
  PatternApp launcher{"Launcher", 1};
  PatternApp timer{"Timer", 10};
  REQUIRE(runtime.registerApp(kHomeAppId, launcher, false));
  REQUIRE(runtime.registerApp(kTimerAppId, timer));
  REQUIRE(runtime.configureHome(kHomeAppId));
  REQUIRE(runtime.begin(kHomeAppId));

  cadenza::MonoFramebuffer output{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas outputCanvas{output};
  runtime.render(outputCanvas);
  const BufferBytes launcherFrame = snapshot(output);

  REQUIRE(runtime.open(kTimerAppId));
  output.clear();
  runtime.render(outputCanvas);
  CHECK(equals(output, launcherFrame));
  CHECK(transition.sourcesStayedImmutable);

  runtime.update(0.17F, {});
  output.clear();
  runtime.render(outputCanvas);
  CHECK(output.pixel(10, 1));
  CHECK(output.pixel(12, 2));
  CHECK_FALSE(output.pixel(1, 1));
  CHECK(transition.sourcesStayedImmutable);
  CHECK(transition.calls == 2);
}

TEST_CASE("cut and blinds strategies have exact endpoints and immutable inputs") {
  for (const cadenza::Transition* transition : {
           static_cast<const cadenza::Transition*>(&cadenza::kCutTransition),
           static_cast<const cadenza::Transition*>(&cadenza::kVenetianBlindsTransition)}) {
    cadenza::MonoFramebuffer outgoing{cadenza::FramebufferProfile::Sharp};
    cadenza::MonoFramebuffer incoming{cadenza::FramebufferProfile::Sharp};
    cadenza::MonoFramebuffer output{cadenza::FramebufferProfile::Sharp};
    cadenza::MonoCanvas outgoingCanvas{outgoing};
    cadenza::MonoCanvas incomingCanvas{incoming};
    cadenza::MonoCanvas outputCanvas{output};
    outgoingCanvas.fillDither({0, 0, outgoing.width(), outgoing.height()},
                              cadenza::kOrderedDither8x8, 16);
    incomingCanvas.fillDither({0, 0, incoming.width(), incoming.height()},
                              cadenza::kOrderedDither8x8, 48);
    const BufferBytes outgoingBefore = snapshot(outgoing);
    const BufferBytes incomingBefore = snapshot(incoming);

    transition->compose(outgoing, incoming, outputCanvas, 0.0F);
    CHECK(equals(output, outgoingBefore));
    transition->compose(outgoing, incoming, outputCanvas, 1.0F);
    CHECK(equals(output, incomingBefore));
    CHECK(equals(outgoing, outgoingBefore));
    CHECK(equals(incoming, incomingBefore));
  }
}

TEST_CASE("handoff transitions explicitly opt into staged midpoint frames") {
  CHECK(cadenza::kCutTransition.frameModel() ==
        cadenza::TransitionFrameModel::Direct);
  CHECK(cadenza::kVenetianBlindsTransition.frameModel() ==
        cadenza::TransitionFrameModel::Direct);
  CHECK(cadenza::kAppLaunchHandoffTransition.frameModel() ==
        cadenza::TransitionFrameModel::Staged);
  CHECK(cadenza::kAppReturnHandoffTransition.frameModel() ==
        cadenza::TransitionFrameModel::Staged);
}

TEST_CASE("staged handoff has exact phase endpoints around the midpoint") {
  cadenza::MonoFramebuffer source{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoFramebuffer bridge{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoFramebuffer output{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas sourceCanvas{source};
  cadenza::MonoCanvas bridgeCanvas{bridge};
  cadenza::MonoCanvas outputCanvas{output};
  sourceCanvas.fillDither({0, 0, source.width(), source.height()},
                          cadenza::kOrderedDither8x8, 16);
  bridgeCanvas.fillDither({0, 0, bridge.width(), bridge.height()},
                          cadenza::kOrderedDither8x8, 48);
  const BufferBytes sourceBytes = snapshot(source);
  const BufferBytes bridgeBytes = snapshot(bridge);

  cadenza::kAppLaunchHandoffTransition.compose(
      source, bridge, outputCanvas, 0.0F);
  CHECK(equals(output, sourceBytes));
  cadenza::kAppLaunchHandoffTransition.compose(
      bridge, source, outputCanvas, 0.5F);
  CHECK(equals(output, bridgeBytes));
  cadenza::kAppReturnHandoffTransition.compose(
      source, bridge, outputCanvas, 0.0F);
  CHECK(equals(output, sourceBytes));
  cadenza::kAppReturnHandoffTransition.compose(
      bridge, source, outputCanvas, 0.5F);
  CHECK(equals(output, bridgeBytes));
}

TEST_CASE("default runtime routes launch and return through staged identities") {
  LaunchProbeApp launcher{"Launcher"};
  LaunchProbeApp app{"App", true};
  cadenza::AppRuntime runtime;
  REQUIRE(runtime.registerApp(kHomeAppId, launcher, false));
  REQUIRE(runtime.registerApp(kTimerAppId, app));
  REQUIRE(runtime.configureHome(kHomeAppId));
  REQUIRE(runtime.begin(kHomeAppId));

  cadenza::MonoFramebuffer output{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{output};
  REQUIRE(runtime.open(kTimerAppId));
  runtime.render(canvas);
  CHECK(app.launchCalls >= 2);
  CHECK(app.enters == 0);
  runtime.update(0.39F, {});
  runtime.render(canvas);
  CHECK(app.lastLaunchProgress > 0.9F);
  CHECK(runtime.currentId() == kHomeAppId);
  runtime.update(0.02F, {});
  runtime.render(canvas);
  CHECK(runtime.currentId() == kTimerAppId);
  CHECK(app.enters == 1);
  CHECK(launcher.exits == 1);
  CHECK(app.updates == 0);
  runtime.update(0.40F, {});
  CHECK_FALSE(runtime.transitioning());

  REQUIRE(runtime.open(kHomeAppId));
  runtime.update(0.21F, {});
  CHECK(runtime.currentId() == kTimerAppId);
  runtime.update(0.02F, {});
  CHECK(runtime.currentId() == kHomeAppId);
  runtime.update(0.22F, {});
  CHECK_FALSE(runtime.transitioning());
  CHECK(app.exits == 1);
  CHECK(launcher.enters == 2);
  CHECK(sizeof(cadenza::AppRuntime) < 25000);
}

TEST_CASE("missing launch renderer falls back without blocking lifecycle") {
  LaunchProbeApp launcher{"Launcher"};
  LaunchProbeApp app{"Fallback", false};
  cadenza::AppRuntime runtime;
  REQUIRE(runtime.registerApp(kHomeAppId, launcher, false));
  REQUIRE(runtime.registerApp(kTimerAppId, app));
  REQUIRE(runtime.configureHome(kHomeAppId));
  REQUIRE(runtime.begin(kHomeAppId));
  REQUIRE(runtime.open(kTimerAppId));
  runtime.update(0.81F, {});
  CHECK_FALSE(runtime.transitioning());
  CHECK(runtime.currentId() == kTimerAppId);
  CHECK(app.enters == 1);
  CHECK(app.launchCalls == 1);
}
