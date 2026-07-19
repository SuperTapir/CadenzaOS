#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "cadenza/core/mono_canvas.h"

static_assert(sizeof(cadenza::BoundedTextResult) <= 512,
              "bounded text results must stay stack-friendly on firmware");

namespace {
class CountingSink final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    ++counts[static_cast<std::size_t>(event.code)];
  }

  std::array<int, 7> counts{};
};

bool inside(cadenza::Rect inner, cadenza::Rect outer) {
  return inner.x >= outer.x && inner.y >= outer.y &&
         inner.x + inner.width <= outer.x + outer.width &&
         inner.y + inner.height <= outer.y + outer.height;
}

cadenza::BoundedTextRequest request(const char* value, cadenza::Rect bounds) {
  cadenza::BoundedTextRequest output;
  output.value = value;
  output.bounds = bounds;
  output.preferredScale = 1;
  output.minimumScale = 1;
  output.align = cadenza::TextAlign::MiddleCenter;
  output.overflow = cadenza::TextOverflowPolicy::Ellipsis;
  output.role = cadenza::TextRole::Caption;
  return output;
}

TEST_CASE("bounded text preserves its explicit typography role") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::Sharp};
  cadenza::MonoCanvas canvas{framebuffer};
  auto input = request("Settings", {10, 10, 140, 36});
  input.role = cadenza::TextRole::Body;

  const auto result = canvas.layoutText(input);
  REQUIRE(result.drawable());
  CHECK(result.role == cadenza::TextRole::Body);
  CHECK(result.renderedBounds.width ==
        canvas.measureText("Settings", cadenza::TextRole::Body).width);
  CHECK(canvas.drawBoundedText(result));
}
}  // namespace

TEST_CASE("bounded text validates requests and preserves exact preferred fits") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  CountingSink diagnostics;
  cadenza::MonoCanvas canvas{framebuffer, &diagnostics};

  const auto exactMetrics = canvas.measureText("A", 2);
  auto exact = request("A", {3, 4, exactMetrics.width, exactMetrics.height});
  exact.preferredScale = 2;
  const auto exactResult = canvas.layoutText(exact);
  CHECK(exactResult.status == cadenza::BoundedTextStatus::Complete);
  CHECK(exactResult.scale == 2);
  CHECK(exactResult.lineCount == 1);
  CHECK(exactResult.renderedBounds.x == 3);
  CHECK(exactResult.renderedBounds.y == 4);
  CHECK(exactResult.renderedBounds.width == exactMetrics.width);
  CHECK(exactResult.renderedBounds.height == exactMetrics.height);

  auto invalid = request(nullptr, {0, 0, 10, 10});
  CHECK(canvas.layoutText(invalid).status ==
        cadenza::BoundedTextStatus::Invalid);
  invalid.value = "A";
  invalid.bounds = {0, 0, 0, 10};
  CHECK(canvas.layoutText(invalid).status ==
        cadenza::BoundedTextStatus::Invalid);
  invalid.bounds = {0, 0, 10, 10};
  invalid.minimumScale = 2;
  invalid.preferredScale = 1;
  CHECK(canvas.layoutText(invalid).status ==
        cadenza::BoundedTextStatus::Invalid);
  CHECK(diagnostics.counts[static_cast<std::size_t>(
            cadenza::DiagnosticCode::InvalidGeometry)] == 3);
}

TEST_CASE("fit selects the largest scale and resolves centered bounds") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  const auto metrics = canvas.measureText("ABC", 1);
  auto input = request("ABC", {10, 10, metrics.width + 2, metrics.height + 4});
  input.preferredScale = 3;
  const auto result = canvas.layoutText(input);

  CHECK(result.status == cadenza::BoundedTextStatus::Scaled);
  CHECK(result.scale == 1);
  CHECK(result.lineCount == 1);
  CHECK(result.renderedBounds.x == 11);
  CHECK(result.renderedBounds.y == 12);
  CHECK(result.renderedBounds.width == metrics.width);
  CHECK(result.renderedBounds.height == metrics.height);
  CHECK(inside(result.renderedBounds, input.bounds));
}

TEST_CASE("bounded rasterization preserves region and framebuffer guard bytes") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  std::fill(framebuffer.data() + framebuffer.sizeBytes(),
            framebuffer.data() + framebuffer.capacityBytes(), 0xA5);
  const auto textMetrics = canvas.measureText("BOUND");
  const auto input = request("BOUND", {20, 30, textMetrics.width + 4,
                                       textMetrics.height});
  const auto result = canvas.boundedText(input);

  CHECK(result.drawable());
  for (int y = 0; y < framebuffer.height(); ++y) {
    for (int x = 0; x < framebuffer.width(); ++x) {
      if (x < input.bounds.x || x >= input.bounds.x + input.bounds.width ||
          y < input.bounds.y || y >= input.bounds.y + input.bounds.height) {
        CHECK_FALSE(framebuffer.pixel(x, y));
      }
    }
  }
  CHECK(std::all_of(framebuffer.data() + framebuffer.sizeBytes(),
                    framebuffer.data() + framebuffer.capacityBytes(),
                    [](std::uint8_t byte) { return byte == 0xA5; }));
}

TEST_CASE("ellipsis is explicit and no-fit regions remain no-ops") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  const int lineHeight = canvas.measureText("A").height;
  auto input = request("A VERY LONG MENU LABEL", {0, 0, 44, lineHeight});
  const auto truncated = canvas.layoutText(input);
  REQUIRE(truncated.status == cadenza::BoundedTextStatus::Truncated);
  REQUIRE(truncated.lineCount == 1);
  CHECK(std::strcmp(truncated.lines[0].value.data() +
                        std::strlen(truncated.lines[0].value.data()) - 3,
                    "...") == 0);
  CHECK(canvas.measureText(truncated.lines[0].value.data(), truncated.scale)
            .width <= input.bounds.width);

  input.bounds = {0, 0, 1, lineHeight};
  const auto noFit = canvas.boundedText(input);
  CHECK(noFit.status == cadenza::BoundedTextStatus::NoFit);
  CHECK(std::all_of(framebuffer.data(),
                    framebuffer.data() + framebuffer.sizeBytes(),
                    [](std::uint8_t byte) { return byte == 0; }));

  input = request("", {0, 0, 44, lineHeight});
  CHECK(canvas.boundedText(input).status == cadenza::BoundedTextStatus::NoFit);
  CHECK(std::all_of(framebuffer.data(),
                    framebuffer.data() + framebuffer.sizeBytes(),
                    [](std::uint8_t byte) { return byte == 0; }));
}

TEST_CASE("wrap honors line capacity and ellipsizes the last visible line") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  const int lineHeight = canvas.measureText("A").height;
  auto input = request("ONE TWO THREE FOUR FIVE",
                       {4, 5, 44, lineHeight * 2});
  input.align = cadenza::TextAlign::TopLeft;
  input.overflow = cadenza::TextOverflowPolicy::Wrap;
  input.maximumLines = 2;
  const auto result = canvas.layoutText(input);

  CHECK(result.status == cadenza::BoundedTextStatus::Wrapped);
  REQUIRE(result.lineCount == 2);
  CHECK(result.truncated);
  const char* finalLine = result.lines[1].value.data();
  REQUIRE(std::strlen(finalLine) >= 3);
  CHECK(std::strcmp(finalLine + std::strlen(finalLine) - 3, "...") == 0);
  CHECK(inside(result.renderedBounds, input.bounds));
}

TEST_CASE("wrap preserves complete content and hard-breaks oversized tokens") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};

  const int lineHeight = canvas.measureText("A").height;
  const int wordWidth = std::max(canvas.measureText("ONE").width,
                                 canvas.measureText("TWO").width);
  auto words = request("ONE TWO", {0, 0, wordWidth, lineHeight * 2});
  words.align = cadenza::TextAlign::TopLeft;
  words.overflow = cadenza::TextOverflowPolicy::Wrap;
  words.maximumLines = 2;
  const auto wordResult = canvas.layoutText(words);
  CHECK(wordResult.status == cadenza::BoundedTextStatus::Wrapped);
  REQUIRE(wordResult.lineCount == 2);
  CHECK_FALSE(wordResult.truncated);
  CHECK(std::strcmp(wordResult.lines[0].value.data(), "ONE") == 0);
  CHECK(std::strcmp(wordResult.lines[1].value.data(), "TWO") == 0);

  auto token = request("ABCDEFGH", {0, 32, 40, lineHeight * 4});
  token.align = cadenza::TextAlign::TopLeft;
  token.overflow = cadenza::TextOverflowPolicy::Wrap;
  token.maximumLines = 4;
  const auto tokenResult = canvas.layoutText(token);
  CHECK(tokenResult.status == cadenza::BoundedTextStatus::Wrapped);
  CHECK(tokenResult.lineCount > 1);
  CHECK_FALSE(tokenResult.truncated);
}

TEST_CASE("marquee phase is deterministic and intentional clipping is quiet") {
  cadenza::MonoFramebuffer first{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoFramebuffer second{cadenza::FramebufferProfile::TEmbed};
  CountingSink firstDiagnostics;
  CountingSink secondDiagnostics;
  cadenza::MonoCanvas firstCanvas{first, &firstDiagnostics};
  cadenza::MonoCanvas secondCanvas{second, &secondDiagnostics};
  const int scaledHeight = firstCanvas.measureText("A", 2).height;
  auto input = request("MARQUEE CONTENT", {10, 10, 44, scaledHeight});
  input.preferredScale = 2;
  input.minimumScale = 2;
  input.overflow = cadenza::TextOverflowPolicy::Marquee;
  input.phase = 0.5F;

  const auto firstResult = firstCanvas.boundedText(input);
  const auto secondResult = secondCanvas.boundedText(input);
  CHECK(firstResult.status == cadenza::BoundedTextStatus::Marquee);
  CHECK(firstResult.lines[0].bounds.x == secondResult.lines[0].bounds.x);
  CHECK(std::equal(first.data(), first.data() + first.sizeBytes(),
                   second.data()));
  CHECK(firstDiagnostics.counts[static_cast<std::size_t>(
            cadenza::DiagnosticCode::ClippedGeometry)] == 0);
  CHECK(firstDiagnostics.counts[static_cast<std::size_t>(
            cadenza::DiagnosticCode::FullyClipped)] == 0);
}

TEST_CASE("extreme or out-of-clip rectangles are rejected without mutation") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  CountingSink diagnostics;
  cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
  const int lineHeight = canvas.measureText("A").height;
  auto input = request("A", {2147483640, 0, 20, lineHeight});
  CHECK(canvas.boundedText(input).status ==
        cadenza::BoundedTextStatus::Invalid);
  canvas.setClip({10, 10, 20, 20});
  input.bounds = {0, 0, 10, lineHeight};
  CHECK(canvas.boundedText(input).status ==
        cadenza::BoundedTextStatus::Invalid);
  CHECK(std::all_of(framebuffer.data(),
                    framebuffer.data() + framebuffer.sizeBytes(),
                    [](std::uint8_t byte) { return byte == 0; }));
}
