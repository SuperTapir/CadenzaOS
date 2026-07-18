#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>

#include "cadenza/core/app_runtime.h"
#include "cadenza/core/transition.h"

namespace {
constexpr cadenza::AppId kHomeAppId{1};
constexpr cadenza::AppId kClockAppId{2};

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
  PatternApp clock{"Clock", 10};
  REQUIRE(runtime.registerApp(kHomeAppId, launcher, false));
  REQUIRE(runtime.registerApp(kClockAppId, clock));
  REQUIRE(runtime.configureHome(kHomeAppId));
  REQUIRE(runtime.begin(kHomeAppId));

  cadenza::MonoFramebuffer output{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas outputCanvas{output};
  runtime.render(outputCanvas);
  const BufferBytes launcherFrame = snapshot(output);

  REQUIRE(runtime.open(kClockAppId));
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
