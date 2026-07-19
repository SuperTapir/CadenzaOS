#include "cadenza/voice/voice_dma_layout.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace cadenza::voice {
namespace {

void extractChannel(const std::int16_t* interleaved, std::size_t frames,
                    VoiceDmaChannelMode channel, std::int16_t* mono) noexcept {
  for (std::size_t frame = 0; frame < frames; ++frame) {
    mono[frame] = channel == VoiceDmaChannelMode::Second
                      ? interleaved[frame * 2 + 1]
                      : interleaved[frame * 2];
  }
}

bool decodeToInterleaved(const void* bytes, std::size_t byteCount,
                         VoiceDmaLayout layout, std::int16_t* interleaved,
                         std::size_t* framesOut) noexcept {
  if (bytes == nullptr || interleaved == nullptr || framesOut == nullptr) {
    return false;
  }
  switch (layout) {
    case VoiceDmaLayout::Int16Interleaved: {
      if (byteCount < 4 || byteCount % 4 != 0) return false;
      const auto count = byteCount / sizeof(std::int16_t);
      if (count % 2 != 0) return false;
      std::memcpy(interleaved, bytes, byteCount);
      *framesOut = count / 2;
      return true;
    }
    case VoiceDmaLayout::Int32Msb16:
    case VoiceDmaLayout::Int32Lsb16: {
      if (byteCount < 8 || byteCount % 8 != 0) return false;
      const auto* words = static_cast<const std::int32_t*>(bytes);
      const std::size_t slots = byteCount / sizeof(std::int32_t);
      if (slots % 2 != 0) return false;
      for (std::size_t i = 0; i < slots; ++i) {
        const std::int32_t word = words[i];
        interleaved[i] = layout == VoiceDmaLayout::Int32Msb16
                             ? static_cast<std::int16_t>(word >> 16)
                             : static_cast<std::int16_t>(word);
      }
      *framesOut = slots / 2;
      return true;
    }
  }
  return false;
}

}  // namespace

VoiceMonoQuality assessMonoChannel(const std::int16_t* mono,
                                   std::size_t sampleCount) noexcept {
  VoiceMonoQuality quality{};
  if (mono == nullptr || sampleCount < 8) return quality;

  double sum = 0.0;
  double sumSq = 0.0;
  for (std::size_t i = 0; i < sampleCount; ++i) {
    const double v = mono[i] / 32768.0;
    sum += v;
    sumSq += v * v;
  }
  const double mean = sum / static_cast<double>(sampleCount);
  const double rms =
      std::sqrt(std::max(0.0, sumSq / static_cast<double>(sampleCount) -
                                  mean * mean));
  quality.rms = static_cast<float>(rms);

  double diffSq = 0.0;
  double num = 0.0;
  double den = 0.0;
  for (std::size_t i = 1; i < sampleCount; ++i) {
    const double a = mono[i - 1] / 32768.0 - mean;
    const double b = mono[i] / 32768.0 - mean;
    const double d = b - a;
    diffSq += d * d;
    num += a * b;
    den += a * a;
  }
  const double hf =
      std::sqrt(diffSq / static_cast<double>(sampleCount - 1));
  quality.hfRatio =
      static_cast<float>(hf / std::max(rms, 1.0e-6));
  quality.ac1 = den > 1.0e-12
                    ? static_cast<float>(num / den)
                    : 0.0F;

  // Prefer speech-like continuity and punish grit. Near-silence scores neutral.
  if (quality.rms < 0.00015F) {
    quality.score = 0.0F;
  } else {
    quality.score = quality.ac1 * 2.0F - quality.hfRatio;
  }
  return quality;
}

VoiceLayoutChoice pickBestVoiceLayout(const void* bytes,
                                      std::size_t byteCount) noexcept {
  VoiceLayoutChoice best{};
  std::int16_t interleaved[512];
  std::int16_t mono[256];
  const VoiceDmaLayout layouts[] = {VoiceDmaLayout::Int16Interleaved,
                                    VoiceDmaLayout::Int32Msb16,
                                    VoiceDmaLayout::Int32Lsb16};
  const VoiceDmaChannelMode channels[] = {VoiceDmaChannelMode::First,
                                          VoiceDmaChannelMode::Second};

  for (const auto layout : layouts) {
    std::size_t frames = 0;
    if (!decodeToInterleaved(bytes, byteCount, layout, interleaved, &frames)) {
      continue;
    }
    if (frames > 256) frames = 256;
    for (const auto channel : channels) {
      extractChannel(interleaved, frames, channel, mono);
      VoiceLayoutChoice candidate;
      candidate.layout = layout;
      candidate.channel = channel;
      candidate.quality = assessMonoChannel(mono, frames);
      if (candidate.quality.score > best.quality.score) {
        best = candidate;
      }
    }
  }
  return best;
}

std::size_t expandVoiceDmaSlots(const void* bytes, std::size_t byteCount,
                                VoiceDmaLayout layout, std::int32_t* slots,
                                std::size_t slotCapacity) noexcept {
  if (bytes == nullptr || slots == nullptr || slotCapacity < 2) return 0;
  std::int16_t interleaved[512];
  std::size_t frames = 0;
  if (!decodeToInterleaved(bytes, byteCount, layout, interleaved, &frames)) {
    return 0;
  }
  const std::size_t slotsNeeded = frames * 2;
  if (slotsNeeded > slotCapacity || frames > 256) return 0;
  for (std::size_t i = 0; i < slotsNeeded; ++i) {
    // Feed LSB-aligned int16 into the normalizer.
    slots[i] = static_cast<std::int32_t>(interleaved[i]);
  }
  return slotsNeeded;
}

}  // namespace cadenza::voice
