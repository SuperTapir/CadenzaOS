#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <filesystem>
#include <fstream>
#include <string>

#include "cadenza/desktop/recording.h"

namespace {
std::filesystem::path freshDirectory(const char* name) {
  const auto path = std::filesystem::temp_directory_path() / name;
  std::filesystem::remove_all(path);
  std::filesystem::create_directories(path);
  return path;
}
}  // namespace

TEST_CASE("recording preserves order skips paused frames and finalizes outputs") {
  const auto directory = freshDirectory("cadenza-recording-order");
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::desktop::RecordingSession recording;
  REQUIRE(recording.start(directory.string(), "demo", framebuffer, true));
  framebuffer.setPixel(0, 0);
  CHECK(recording.capture(1, true, framebuffer) ==
        cadenza::desktop::CaptureResult::Captured);
  CHECK(recording.capture(2, false, framebuffer) ==
        cadenza::desktop::CaptureResult::SkippedPaused);
  framebuffer.setPixel(1, 0);
  CHECK(recording.capture(3, true, framebuffer) ==
        cadenza::desktop::CaptureResult::Captured);
  REQUIRE(recording.stop());
  CHECK_FALSE(recording.active());
  CHECK(std::filesystem::exists(directory / "demo-f00000001.png"));
  CHECK_FALSE(std::filesystem::exists(directory / "demo-f00000002.png"));
  CHECK(std::filesystem::exists(directory / "demo-f00000003.png"));
  REQUIRE(std::filesystem::file_size(directory / "demo.gif") > 6);
  std::ifstream gif{directory / "demo.gif", std::ios::binary};
  char signature[6]{};
  gif.read(signature, sizeof(signature));
  CHECK(std::string(signature, sizeof(signature)) == "GIF89a");
  std::filesystem::remove_all(directory);
}

TEST_CASE("out-of-order failure cleans partial session artifacts") {
  const auto directory = freshDirectory("cadenza-recording-cleanup");
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::desktop::RecordingSession recording;
  REQUIRE(recording.start(directory.string(), "broken", framebuffer, true));
  REQUIRE(recording.capture(5, true, framebuffer) ==
          cadenza::desktop::CaptureResult::Captured);
  CHECK(recording.capture(4, true, framebuffer) ==
        cadenza::desktop::CaptureResult::Error);
  CHECK_FALSE(recording.active());
  CHECK(std::filesystem::is_empty(directory));
  std::filesystem::remove_all(directory);
}
