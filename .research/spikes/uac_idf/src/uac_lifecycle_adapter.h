#pragma once

#include <cstdint>

#include "cadenza/voice/usb_voice_session_bridge.h"

namespace cadenza::idf {

void installUacLifecycleAdapter(voice::UsbVoiceSessionBridge& bridge,
                                std::uint8_t microphoneInterface) noexcept;
void uacMounted() noexcept;
void uacUnmounted() noexcept;
void uacSuspended() noexcept;
void uacResumed() noexcept;

}  // namespace cadenza::idf
