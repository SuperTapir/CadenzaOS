#pragma once

#include <array>
#include <cstdint>

#include "cadenza/core/mono_framebuffer.h"
#include "esp_err.h"

namespace cadenza::idf {

// ESP-IDF 5 esp_lcd owner for the original T-Embed ST7789 panel.
class TEmbedDisplay {
 public:
  TEmbedDisplay() noexcept = default;
  ~TEmbedDisplay();

  TEmbedDisplay(const TEmbedDisplay&) = delete;
  TEmbedDisplay& operator=(const TEmbedDisplay&) = delete;

  esp_err_t start() noexcept;
  bool present(const MonoFramebuffer& framebuffer) noexcept;

 private:
  static constexpr std::size_t kRowsPerTransfer = 8;
  static constexpr std::size_t kWidth = 320;

  void release() noexcept;

  void* panelIo_ = nullptr;
  void* panel_ = nullptr;
  void* transferDone_ = nullptr;
  bool busInitialized_ = false;
  std::array<std::uint16_t, kWidth * kRowsPerTransfer> transfer_{};
};

}  // namespace cadenza::idf
