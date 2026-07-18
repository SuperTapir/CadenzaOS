#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/voice/voice_input.h"

namespace cadenza::voice {

inline constexpr std::size_t kUsbVoiceSamplesPerPacket = 48;
inline constexpr std::size_t kUsbVoicePacketBytes =
    kUsbVoiceSamplesPerPacket * sizeof(std::int16_t);

struct UsbVoicePacket {
  std::array<std::int16_t, kUsbVoiceSamplesPerPacket> samples{};
};

struct UsbVoicePacketizerDiagnostics {
  std::uint32_t packets = 0;
  std::uint32_t underflows = 0;
  std::uint32_t activations = 0;
  std::uint32_t deactivations = 0;
};

class UsbVoicePacketizer {
 public:
  explicit UsbVoicePacketizer(VoiceCaptureCoordinator& capture) noexcept
      : capture_(&capture) {}

  UsbVoicePacketizer(const UsbVoicePacketizer&) = delete;
  UsbVoicePacketizer& operator=(const UsbVoicePacketizer&) = delete;

  void setConnected(bool connected) noexcept;
  void setAltSettingActive(bool active) noexcept;
  bool nextPacket(UsbVoicePacket& packet) noexcept;
  void shutdown() noexcept;

  bool streaming() const noexcept { return streaming_; }
  const UsbVoicePacketizerDiagnostics& diagnostics() const noexcept {
    return diagnostics_;
  }

 private:
  void reconcile() noexcept;
  void resetCurrentBlock() noexcept;

  VoiceCaptureCoordinator* capture_ = nullptr;
  bool connected_ = false;
  bool altSettingActive_ = false;
  bool streaming_ = false;
  VoiceBlock currentBlock_{};
  std::size_t currentOffset_ = kVoiceSamplesPerBlock;
  UsbVoicePacketizerDiagnostics diagnostics_{};
};

}  // namespace cadenza::voice
