#pragma once

#include <atomic>
#include <cstdint>

#include "cadenza/voice/usb_voice_packetizer.h"

namespace cadenza::voice {

struct UsbVoiceSessionBridgeDiagnostics {
  std::uint32_t lifecycleChanges = 0;
  std::uint32_t synchronizations = 0;
};

// Control callbacks publish only atomics. The UAC data consumer calls
// synchronize() before packetization, so partial packet state remains
// single-owner. A close/disconnect clears capture intent immediately; a
// generation change forces a flush even if close+open happened between reads.
class UsbVoiceSessionBridge {
 public:
  explicit UsbVoiceSessionBridge(VoiceCaptureCoordinator& capture) noexcept
      : capture_(&capture) {}

  UsbVoiceSessionBridge(const UsbVoiceSessionBridge&) = delete;
  UsbVoiceSessionBridge& operator=(const UsbVoiceSessionBridge&) = delete;

  void onConnectedFromCallback(bool connected) noexcept;
  void onAltSettingFromCallback(bool active) noexcept;
  bool synchronize(UsbVoicePacketizer& packetizer) noexcept;

  bool connected() const noexcept {
    return connected_.load(std::memory_order_acquire);
  }
  bool altSettingActive() const noexcept {
    return altSettingActive_.load(std::memory_order_acquire);
  }
  UsbVoiceSessionBridgeDiagnostics diagnostics() const noexcept {
    return {lifecycleChanges_.load(std::memory_order_relaxed),
            synchronizations_.load(std::memory_order_relaxed)};
  }

 private:
  void changed() noexcept;
  void deactivateCapture() noexcept;

  VoiceCaptureCoordinator* capture_ = nullptr;
  std::atomic<bool> connected_{false};
  std::atomic<bool> altSettingActive_{false};
  std::atomic<std::uint32_t> generation_{0};
  std::atomic<std::uint32_t> lifecycleChanges_{0};
  std::atomic<std::uint32_t> synchronizations_{0};
  std::uint32_t appliedGeneration_ = 0;
};

}  // namespace cadenza::voice
