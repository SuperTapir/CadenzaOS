#define GIF_IMPL
#include "gif.h"

#include "cadenza/desktop/recording.h"

#include <cstdio>
#include <filesystem>
#include <new>
#include <vector>

#include "cadenza/desktop/png_writer.h"

namespace cadenza::desktop {
namespace {
std::string framePath(const std::string& directory,
                      const std::string& basename, FrameIndex frame) {
  char filename[96];
  std::snprintf(filename, sizeof(filename), "%s-f%08llu.png", basename.c_str(),
                static_cast<unsigned long long>(frame));
  return (std::filesystem::path(directory) / filename).string();
}

void makeRgba(const MonoFramebuffer& framebuffer,
              std::vector<std::uint8_t>& rgba) {
  rgba.resize(static_cast<std::size_t>(framebuffer.width()) *
              framebuffer.height() * 4U);
  for (std::int32_t y = 0; y < framebuffer.height(); ++y) {
    for (std::int32_t x = 0; x < framebuffer.width(); ++x) {
      const std::uint8_t value = framebuffer.pixel(x, y) ? 0 : 255;
      const std::size_t offset =
          (static_cast<std::size_t>(y) * framebuffer.width() + x) * 4U;
      rgba[offset] = value;
      rgba[offset + 1] = value;
      rgba[offset + 2] = value;
      rgba[offset + 3] = 255;
    }
  }
}
}  // namespace

RecordingSession::~RecordingSession() {
  if (active_) cancel();
}

bool RecordingSession::start(const std::string& directory,
                             const std::string& basename,
                             const MonoFramebuffer& framebuffer,
                             bool convenienceGif) {
  if (active_ || directory.empty() || basename.empty() || !framebuffer.valid()) {
    return false;
  }
  directory_ = directory;
  basename_ = basename;
  width_ = framebuffer.width();
  height_ = framebuffer.height();
  lastFrame_ = 0;
  capturedFrames_ = 0;
  hasLastFrame_ = false;
  createdPaths_.clear();
  std::error_code error;
  std::filesystem::create_directories(directory_, error);
  if (error) return false;
  active_ = true;

  if (convenienceGif) {
    auto* writer = new (std::nothrow) GifWriter{};
    if (!writer) return fail();
    gifWriter_ = writer;
    const std::string path =
        (std::filesystem::path(directory_) / (basename_ + ".gif")).string();
    if (!GifBegin(writer, path.c_str(), width_, height_, 2, 1, false)) {
      return fail();
    }
    createdPaths_.push_back(path);
  }
  return true;
}

CaptureResult RecordingSession::capture(
    FrameIndex frame, bool simulationAdvanced,
    const MonoFramebuffer& framebuffer) {
  if (!active_) return CaptureResult::Error;
  if (!simulationAdvanced) return CaptureResult::SkippedPaused;
  if ((hasLastFrame_ && frame <= lastFrame_) || framebuffer.width() != width_ ||
      framebuffer.height() != height_) {
    fail();
    return CaptureResult::Error;
  }

  const std::string pngPath = framePath(directory_, basename_, frame);
  if (!writePng(pngPath, framebuffer)) {
    fail();
    return CaptureResult::Error;
  }
  createdPaths_.push_back(pngPath);
  if (gifWriter_) {
    std::vector<std::uint8_t> rgba;
    makeRgba(framebuffer, rgba);
    if (!GifWriteFrame(static_cast<GifWriter*>(gifWriter_), rgba.data(), width_,
                       height_, 2, 1, false)) {
      fail();
      return CaptureResult::Error;
    }
  }
  lastFrame_ = frame;
  hasLastFrame_ = true;
  ++capturedFrames_;
  return CaptureResult::Captured;
}

bool RecordingSession::stop() {
  if (!active_) return false;
  bool result = true;
  if (gifWriter_) {
    result = GifEnd(static_cast<GifWriter*>(gifWriter_));
    delete static_cast<GifWriter*>(gifWriter_);
    gifWriter_ = nullptr;
  }
  active_ = false;
  if (!result) {
    for (const auto& path : createdPaths_) {
      std::error_code ignored;
      std::filesystem::remove(path, ignored);
    }
    createdPaths_.clear();
  }
  return result;
}

void RecordingSession::cancel() {
  if (gifWriter_) {
    GifEnd(static_cast<GifWriter*>(gifWriter_));
    delete static_cast<GifWriter*>(gifWriter_);
    gifWriter_ = nullptr;
  }
  for (const auto& path : createdPaths_) {
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
  }
  createdPaths_.clear();
  active_ = false;
}

bool RecordingSession::fail() {
  cancel();
  return false;
}

}  // namespace cadenza::desktop
