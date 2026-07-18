#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/mono_canvas.h"

namespace {
class CountingSink final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    ++counts[static_cast<std::size_t>(event.code)];
  }
  std::array<int, 7> counts{};
};

void checkPattern(const cadenza::MonoFramebuffer& framebuffer, int x, int y,
                  const char* const* rows, int height) {
  for (int row = 0; row < height; ++row) {
    for (int column = 0; rows[row][column] != '\0'; ++column) {
      CAPTURE(row);
      CAPTURE(column);
      CHECK(framebuffer.pixel(x + column, y + row) == (rows[row][column] == '#'));
    }
  }
}
}  // namespace

TEST_CASE("pixel line and rectangle golden bytes are deterministic") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};

  canvas.pixel(0, 0);
  canvas.line(1, 0, 6, 0);
  CHECK(framebuffer.data()[0] == 0xFE);

  canvas.clear();
  canvas.rect(1, 1, 4, 3);
  CHECK(framebuffer.data()[framebuffer.stride()] == 0x78);
  CHECK(framebuffer.data()[framebuffer.stride() * 2] == 0x48);
  CHECK(framebuffer.data()[framebuffer.stride() * 3] == 0x78);

  canvas.clear();
  canvas.fillRect(1, 1, 4, 3);
  CHECK(framebuffer.data()[framebuffer.stride()] == 0x78);
  CHECK(framebuffer.data()[framebuffer.stride() * 2] == 0x78);
  CHECK(framebuffer.data()[framebuffer.stride() * 3] == 0x78);
}

TEST_CASE("circle and filled circle match approved integer golden patterns") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  const char* circle[] = {".###.", "#...#", "#...#", "#...#", ".###."};
  const char* disc[] = {".###.", "#####", "#####", "#####", ".###."};

  canvas.circle(4, 4, 2);
  checkPattern(framebuffer, 2, 2, circle, 5);

  canvas.clear();
  canvas.fillCircle(4, 4, 2);
  checkPattern(framebuffer, 2, 2, disc, 5);
}

TEST_CASE("half-open clip preserves visible pixels and guard storage") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  CountingSink diagnostics;
  cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
  std::fill(framebuffer.data() + framebuffer.sizeBytes(),
            framebuffer.data() + framebuffer.capacityBytes(), 0xA5);

  canvas.setClip({2, 2, 3, 3});
  canvas.fillRect(-10, -10, 30, 30);

  for (int y = 0; y < 7; ++y) {
    for (int x = 0; x < 7; ++x) {
      CHECK(framebuffer.pixel(x, y) == (x >= 2 && x < 5 && y >= 2 && y < 5));
    }
  }
  CHECK(diagnostics.counts[static_cast<std::size_t>(
            cadenza::DiagnosticCode::ClippedGeometry)] == 1);
  CHECK(std::all_of(framebuffer.data() + framebuffer.sizeBytes(),
                    framebuffer.data() + framebuffer.capacityBytes(),
                    [](std::uint8_t byte) { return byte == 0xA5; }));
}

TEST_CASE("invalid and fully clipped primitives are observable no-ops") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  CountingSink diagnostics;
  cadenza::MonoCanvas canvas{framebuffer, &diagnostics};

  canvas.fillRect(1, 1, 0, 4);
  canvas.circle(4, 4, -1);
  canvas.pixel(-1, -1);
  canvas.line(-5, -5, -1, -1);

  CHECK(diagnostics.counts[static_cast<std::size_t>(
            cadenza::DiagnosticCode::InvalidGeometry)] == 2);
  CHECK(diagnostics.counts[static_cast<std::size_t>(
            cadenza::DiagnosticCode::FullyClipped)] == 2);
  CHECK(std::all_of(framebuffer.data(),
                    framebuffer.data() + framebuffer.sizeBytes(),
                    [](std::uint8_t byte) { return byte == 0; }));
}

TEST_CASE("degenerate line and zero-radius circles include their endpoint") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  canvas.line(3, 3, 3, 3);
  canvas.circle(5, 5, 0);
  canvas.fillCircle(7, 7, 0);
  CHECK(framebuffer.pixel(3, 3));
  CHECK(framebuffer.pixel(5, 5));
  CHECK(framebuffer.pixel(7, 7));
}

TEST_CASE("approved font exposes stable metrics and alignment") {
  cadenza::MonoFramebuffer topLeftBuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoFramebuffer centeredBuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas topLeft{topLeftBuffer};
  cadenza::MonoCanvas centered{centeredBuffer};

  const auto metrics = topLeft.measureText("A", 1);
  CHECK(metrics.width == 5);
  CHECK(metrics.height == 9);
  CHECK(metrics.ascent == 7);
  CHECK(metrics.descent == -2);

  topLeft.text("A", 18, 16, 1, true, cadenza::TextAlign::TopLeft);
  centered.text("A", 20, 20, 1, true, cadenza::TextAlign::MiddleCenter);
  CHECK(std::equal(topLeftBuffer.data(),
                   topLeftBuffer.data() + topLeftBuffer.sizeBytes(),
                   centeredBuffer.data()));
}

TEST_CASE("font integer scaling uses exact nearest-neighbor blocks") {
  cadenza::MonoFramebuffer normalBuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoFramebuffer scaledBuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas normal{normalBuffer};
  cadenza::MonoCanvas scaled{scaledBuffer};
  normal.text("A", 0, 0, 1);
  scaled.text("A", 0, 0, 2);

  for (int y = 0; y < 10; ++y) {
    for (int x = 0; x < 6; ++x) {
      const bool source = normalBuffer.pixel(x, y);
      CHECK(scaledBuffer.pixel(x * 2, y * 2) == source);
      CHECK(scaledBuffer.pixel(x * 2 + 1, y * 2) == source);
      CHECK(scaledBuffer.pixel(x * 2, y * 2 + 1) == source);
      CHECK(scaledBuffer.pixel(x * 2 + 1, y * 2 + 1) == source);
    }
  }
}

TEST_CASE("invalid text scale is diagnosed without drawing") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  CountingSink diagnostics;
  cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
  canvas.text("A", 0, 0, 0);
  CHECK(diagnostics.counts[static_cast<std::size_t>(
            cadenza::DiagnosticCode::InvalidGeometry)] == 1);
  CHECK(std::all_of(framebuffer.data(),
                    framebuffer.data() + framebuffer.sizeBytes(),
                    [](std::uint8_t byte) { return byte == 0; }));
}

TEST_CASE("bitmap copy set-black and xor composition have distinct semantics") {
  const std::uint8_t pixels[] = {0xA0, 0x60};  // 101 / 011
  const cadenza::BitmapView bitmap{pixels, 3, 2, 1};
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};

  canvas.fillRect(10, 10, 3, 2, true);
  canvas.drawBitmap(bitmap, {0, 0, 3, 2}, 10, 10,
                    {cadenza::BitmapComposition::Copy, false, false});
  CHECK(framebuffer.pixel(10, 10));
  CHECK_FALSE(framebuffer.pixel(11, 10));
  CHECK(framebuffer.pixel(12, 10));
  CHECK_FALSE(framebuffer.pixel(10, 11));
  CHECK(framebuffer.pixel(11, 11));
  CHECK(framebuffer.pixel(12, 11));

  canvas.clear();
  canvas.pixel(11, 10);
  canvas.drawBitmap(bitmap, {0, 0, 3, 2}, 10, 10,
                    {cadenza::BitmapComposition::SetBlack, false, false});
  CHECK(framebuffer.pixel(11, 10));
  CHECK(framebuffer.pixel(10, 10));
  CHECK(framebuffer.pixel(12, 10));

  canvas.drawBitmap(bitmap, {0, 0, 3, 2}, 10, 10,
                    {cadenza::BitmapComposition::Xor, false, false});
  CHECK_FALSE(framebuffer.pixel(10, 10));
  CHECK(framebuffer.pixel(11, 10));
  CHECK_FALSE(framebuffer.pixel(12, 10));
}

TEST_CASE("bitmap source clipping translation and flips preserve mapping") {
  const std::uint8_t pixels[] = {0x80, 0x40};  // 10 / 01
  const cadenza::BitmapView bitmap{pixels, 2, 2, 1};
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};

  canvas.drawBitmap(bitmap, {-1, 0, 3, 2}, 20, 30,
                    {cadenza::BitmapComposition::Copy, true, true});
  CHECK(framebuffer.pixel(21, 30));
  CHECK_FALSE(framebuffer.pixel(22, 30));
  CHECK_FALSE(framebuffer.pixel(21, 31));
  CHECK(framebuffer.pixel(22, 31));
  CHECK_FALSE(framebuffer.pixel(20, 30));
  CHECK_FALSE(framebuffer.pixel(20, 31));
}

TEST_CASE("overlapping framebuffer bitmap copy behaves like a pixel memmove") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  canvas.pixel(0, 0);
  canvas.pixel(2, 0);
  canvas.pixel(4, 0);
  const cadenza::BitmapView self{framebuffer.data(), framebuffer.width(),
                                 framebuffer.height(), framebuffer.stride()};

  canvas.drawBitmap(self, {0, 0, 8, 1}, 2, 0);
  const bool expected[] = {true, false, true, false, true, false, false, false};
  for (int x = 0; x < 8; ++x) {
    CHECK(framebuffer.pixel(x + 2, 0) == expected[x]);
  }
}

TEST_CASE("region inversion is clipped and exactly reversible") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  canvas.pixel(2, 2);
  std::array<std::uint8_t, cadenza::MonoFramebuffer::kCapacityBytes> original{};
  std::copy_n(framebuffer.data(), framebuffer.sizeBytes(), original.data());

  canvas.setClip({1, 1, 3, 3});
  canvas.invert({-5, -5, 20, 20});
  CHECK_FALSE(framebuffer.pixel(2, 2));
  CHECK(framebuffer.pixel(1, 1));
  CHECK_FALSE(framebuffer.pixel(0, 0));
  canvas.invert({-5, -5, 20, 20});
  CHECK(std::equal(framebuffer.data(),
                   framebuffer.data() + framebuffer.sizeBytes(),
                   original.data()));
}

TEST_CASE("ordered dither coverage is exact over one tile") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};

  for (const std::uint8_t coverage : {std::uint8_t{0}, std::uint8_t{16},
                                      std::uint8_t{32}, std::uint8_t{64}}) {
    canvas.fillDither({0, 0, 8, 8}, cadenza::kOrderedDither8x8, coverage, 0, 0);
    int blackPixels = 0;
    for (int y = 0; y < 8; ++y) {
      for (int x = 0; x < 8; ++x) {
        blackPixels += framebuffer.pixel(x, y) ? 1 : 0;
      }
    }
    CHECK(blackPixels == coverage);
  }
}

TEST_CASE("dither phase changes arrangement but preserves coverage deterministically") {
  cadenza::MonoFramebuffer first{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoFramebuffer second{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoFramebuffer replay{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas firstCanvas{first};
  cadenza::MonoCanvas secondCanvas{second};
  cadenza::MonoCanvas replayCanvas{replay};

  firstCanvas.fillDither({0, 0, 8, 8}, cadenza::kOrderedDither8x8, 24, 0, 0);
  secondCanvas.fillDither({0, 0, 8, 8}, cadenza::kOrderedDither8x8, 24, 1, 1);
  replayCanvas.fillDither({0, 0, 8, 8}, cadenza::kOrderedDither8x8, 24, 1, 1);

  int firstCount = 0;
  int secondCount = 0;
  bool differs = false;
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      firstCount += first.pixel(x, y) ? 1 : 0;
      secondCount += second.pixel(x, y) ? 1 : 0;
      differs = differs || first.pixel(x, y) != second.pixel(x, y);
      CHECK(second.pixel(x, y) == replay.pixel(x, y));
    }
  }
  CHECK(firstCount == 24);
  CHECK(secondCount == 24);
  CHECK(differs);
}

TEST_CASE("custom dither tables and clipped fills use the same explicit contract") {
  cadenza::DitherPattern8x8 rows{};
  for (std::size_t index = 0; index < rows.thresholds.size(); ++index) {
    rows.thresholds[index] = static_cast<std::uint8_t>(index);
  }
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  CountingSink diagnostics;
  cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
  canvas.setClip({2, 2, 4, 4});
  canvas.fillDither({0, 0, 8, 8}, rows, 8, -2, -2);

  CHECK(diagnostics.counts[static_cast<std::size_t>(
            cadenza::DiagnosticCode::ClippedGeometry)] == 1);
  CHECK_FALSE(framebuffer.pixel(0, 0));
  CHECK(framebuffer.pixel(2, 2));
  CHECK_FALSE(framebuffer.pixel(2, 3));
}

TEST_CASE("invalid dither coverage is diagnosed without mutation") {
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  CountingSink diagnostics;
  cadenza::MonoCanvas canvas{framebuffer, &diagnostics};
  canvas.fillDither({0, 0, 8, 8}, cadenza::kOrderedDither8x8, 65, 0, 0);
  CHECK(diagnostics.counts[static_cast<std::size_t>(
            cadenza::DiagnosticCode::InvalidGeometry)] == 1);
  CHECK(std::all_of(framebuffer.data(),
                    framebuffer.data() + framebuffer.sizeBytes(),
                    [](std::uint8_t byte) { return byte == 0; }));
}

TEST_CASE("off-screen framebuffer composition never mutates source canvases") {
  cadenza::MonoFramebuffer outgoing{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoFramebuffer incoming{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoFramebuffer output{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas outgoingCanvas{outgoing};
  cadenza::MonoCanvas incomingCanvas{incoming};
  cadenza::MonoCanvas outputCanvas{output};
  outgoingCanvas.fillDither({0, 0, 16, 8}, cadenza::kOrderedDither8x8, 16);
  incomingCanvas.fillDither({0, 0, 16, 8}, cadenza::kOrderedDither8x8, 48);

  std::array<std::uint8_t, cadenza::MonoFramebuffer::kCapacityBytes>
      outgoingBefore{};
  std::array<std::uint8_t, cadenza::MonoFramebuffer::kCapacityBytes>
      incomingBefore{};
  std::copy_n(outgoing.data(), outgoing.sizeBytes(), outgoingBefore.data());
  std::copy_n(incoming.data(), incoming.sizeBytes(), incomingBefore.data());

  outputCanvas.drawFramebuffer(outgoing, {0, 0, 16, 8}, 0, 0);
  outputCanvas.drawFramebuffer(incoming, {8, 0, 8, 8}, 8, 0);

  CHECK(std::equal(outgoing.data(), outgoing.data() + outgoing.sizeBytes(),
                   outgoingBefore.data()));
  CHECK(std::equal(incoming.data(), incoming.data() + incoming.sizeBytes(),
                   incomingBefore.data()));
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 16; ++x) {
      const bool expected = x < 8 ? outgoing.pixel(x, y) : incoming.pixel(x, y);
      CHECK(output.pixel(x, y) == expected);
    }
  }
}
