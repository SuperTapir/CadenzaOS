#pragma once

#include <cstddef>
#include <cstdint>

#include "cadenza/voice/voice_dma_normalizer.h"

namespace cadenza::voice {

enum class VoiceDmaLayout : std::uint8_t {
  Int16Interleaved = 0,
  Int32Msb16 = 1,
  Int32Lsb16 = 2,
};

struct VoiceMonoQuality {
  float rms = 0.0F;
  float hfRatio = 0.0F;
  float ac1 = 0.0F;
  float score = -1.0F;
};

struct VoiceLayoutChoice {
  VoiceDmaLayout layout = VoiceDmaLayout::Int32Msb16;
  VoiceDmaChannelMode channel = VoiceDmaChannelMode::First;
  VoiceMonoQuality quality{};
};

// Score one mono channel extracted from interleaved stereo samples.
VoiceMonoQuality assessMonoChannel(const std::int16_t* mono,
                                   std::size_t sampleCount) noexcept;

// Choose DMA unpack + L/R channel from a raw I²S byte window. Used after each
// capture start so RST/I²S packing lotteries pick a clean decode for the session.
VoiceLayoutChoice pickBestVoiceLayout(const void* bytes,
                                      std::size_t byteCount) noexcept;

// Expand a raw DMA window into int32 slots for VoiceDmaNormalizer using the
// chosen layout. Returns slot count (0 on failure).
std::size_t expandVoiceDmaSlots(const void* bytes, std::size_t byteCount,
                                VoiceDmaLayout layout, std::int32_t* slots,
                                std::size_t slotCapacity) noexcept;

}  // namespace cadenza::voice
