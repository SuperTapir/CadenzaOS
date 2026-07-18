#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "cadenza/core/core_types.h"
#include "cadenza/core/mono_framebuffer.h"

namespace cadenza::desktop {

enum class CaptureResult {
  Captured,
  SkippedPaused,
  Error,
};

class RecordingSession {
 public:
  RecordingSession() = default;
  ~RecordingSession();
  RecordingSession(const RecordingSession&) = delete;
  RecordingSession& operator=(const RecordingSession&) = delete;

  bool start(const std::string& directory, const std::string& basename,
             const MonoFramebuffer& framebuffer, bool convenienceGif);
  CaptureResult capture(FrameIndex frame, bool simulationAdvanced,
                        const MonoFramebuffer& framebuffer);
  bool stop();
  void cancel();

  bool active() const noexcept { return active_; }
  std::size_t capturedFrames() const noexcept { return capturedFrames_; }

 private:
  bool fail();

  std::string directory_;
  std::string basename_;
  std::vector<std::string> createdPaths_;
  void* gifWriter_ = nullptr;
  std::int16_t width_ = 0;
  std::int16_t height_ = 0;
  FrameIndex lastFrame_ = 0;
  std::size_t capturedFrames_ = 0;
  bool hasLastFrame_ = false;
  bool active_ = false;
};

}  // namespace cadenza::desktop
