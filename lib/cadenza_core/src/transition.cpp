#include "cadenza/core/transition.h"

#include <algorithm>
#include <cstdint>
#include <cmath>

namespace cadenza {
namespace {
void copyFrame(const MonoFramebuffer& source, MonoCanvas& output) noexcept {
  output.resetClip();
  output.drawFramebuffer(source, {0, 0, source.width(), source.height()}, 0, 0);
}

float normalized(float progress) noexcept {
  return std::max(0.0F, std::min(1.0F, progress));
}

// Pack incoming where selector is true, outgoing otherwise. Avoids per-pixel
// MonoCanvas::pixel / MonoFramebuffer::pixel call overhead on T-Embed.
template <typename Selector>
void selectPixelsPacked(const MonoFramebuffer& outgoing,
                        const MonoFramebuffer& incoming, MonoCanvas& output,
                        Selector&& useIncoming) noexcept {
  output.resetClip();
  MonoFramebuffer& destination = output.framebuffer();
  const std::int32_t width = destination.width();
  const std::int32_t height = destination.height();
  const std::size_t stride = destination.stride();
  std::uint8_t* out = destination.data();
  const std::uint8_t* srcOut = outgoing.data();
  const std::uint8_t* srcIn = incoming.data();
  for (std::int32_t y = 0; y < height; ++y) {
    const std::size_t row = static_cast<std::size_t>(y) * stride;
    for (std::size_t byteIndex = 0; byteIndex < stride; ++byteIndex) {
      std::uint8_t mask = 0;
      const std::int32_t baseX = static_cast<std::int32_t>(byteIndex) * 8;
      for (std::int32_t bit = 0; bit < 8; ++bit) {
        const std::int32_t x = baseX + bit;
        if (x >= width) break;
        if (useIncoming(x, y)) {
          mask = static_cast<std::uint8_t>(mask | (0x80U >> bit));
        }
      }
      const std::size_t index = row + byteIndex;
      out[index] = static_cast<std::uint8_t>((srcIn[index] & mask) |
                                             (srcOut[index] &
                                              static_cast<std::uint8_t>(~mask)));
    }
  }
}

// Ordered-dither dissolve without per-pixel MonoCanvas/MonoFramebuffer calls.
void dissolvePacked(const MonoFramebuffer& outgoing,
                    const MonoFramebuffer& incoming, MonoCanvas& output,
                    std::uint8_t coverage) noexcept {
  selectPixelsPacked(outgoing, incoming, output,
                     [coverage](std::int32_t x, std::int32_t y) {
                       return kOrderedDither8x8
                                  .thresholds[(y & 7) * 8 + (x & 7)] <
                              coverage;
                     });
}

void dipPacked(const MonoFramebuffer& outgoing,
               const MonoFramebuffer& incoming, MonoCanvas& output,
               float progress) noexcept {
  output.resetClip();
  MonoFramebuffer& destination = output.framebuffer();
  const std::int32_t width = destination.width();
  const std::int32_t height = destination.height();
  const std::size_t stride = destination.stride();
  std::uint8_t* out = destination.data();
  const std::uint8_t* srcOut = outgoing.data();
  const std::uint8_t* srcIn = incoming.data();
  const float p = normalized(progress);
  if (p == 0.5F) {
    destination.clear(true);
    return;
  }
  if (p < 0.5F) {
    const std::uint8_t coverage =
        static_cast<std::uint8_t>(p * 2.0F * 64.0F);
    for (std::int32_t y = 0; y < height; ++y) {
      const std::size_t row = static_cast<std::size_t>(y) * stride;
      const std::uint8_t* thresholdRow =
          &kOrderedDither8x8.thresholds[(y & 7) * 8];
      for (std::size_t byteIndex = 0; byteIndex < stride; ++byteIndex) {
        std::uint8_t ditherMask = 0;
        const std::int32_t baseX = static_cast<std::int32_t>(byteIndex) * 8;
        for (std::int32_t bit = 0; bit < 8; ++bit) {
          const std::int32_t x = baseX + bit;
          if (x >= width) break;
          if (thresholdRow[x & 7] < coverage) {
            ditherMask =
                static_cast<std::uint8_t>(ditherMask | (0x80U >> bit));
          }
        }
        const std::size_t index = row + byteIndex;
        // black = outgoing || dither — same as original Dip mid-phase.
        out[index] = static_cast<std::uint8_t>(srcOut[index] | ditherMask);
      }
    }
    return;
  }
  const std::uint8_t reveal =
      static_cast<std::uint8_t>((p - 0.5F) * 2.0F * 64.0F);
  for (std::int32_t y = 0; y < height; ++y) {
    const std::size_t row = static_cast<std::size_t>(y) * stride;
    const std::uint8_t* thresholdRow =
        &kOrderedDither8x8.thresholds[(y & 7) * 8];
    for (std::size_t byteIndex = 0; byteIndex < stride; ++byteIndex) {
      std::uint8_t revealMask = 0;
      const std::int32_t baseX = static_cast<std::int32_t>(byteIndex) * 8;
      for (std::int32_t bit = 0; bit < 8; ++bit) {
        const std::int32_t x = baseX + bit;
        if (x >= width) break;
        if (thresholdRow[x & 7] < reveal) {
          revealMask =
              static_cast<std::uint8_t>(revealMask | (0x80U >> bit));
        }
      }
      const std::size_t index = row + byteIndex;
      // Unrevealed stays black; revealed takes incoming.
      out[index] = static_cast<std::uint8_t>(
          (srcIn[index] & revealMask) |
          static_cast<std::uint8_t>(~revealMask));
    }
  }
}

bool endpoint(const MonoFramebuffer& outgoing, const MonoFramebuffer& incoming,
              MonoCanvas& output, float progress) noexcept {
  if (progress <= 0.0F) {
    copyFrame(outgoing, output);
    return true;
  }
  if (progress >= 1.0F) {
    copyFrame(incoming, output);
    return true;
  }
  return false;
}
}  // namespace

const CutTransition kCutTransition;
const VenetianBlindsTransition kVenetianBlindsTransition;
const DipTransition kDipTransition;
const HorizontalWipeTransition kHorizontalWipeTransition;
const DiagonalWipeTransition kDiagonalWipeTransition;
const IrisTransition kIrisTransition;
const CheckerDissolveTransition kCheckerDissolveTransition;
const AppLaunchHandoffTransition kAppLaunchHandoffTransition;
const AppReturnHandoffTransition kAppReturnHandoffTransition;

namespace {
void dissolveFrames(const MonoFramebuffer& outgoing,
                    const MonoFramebuffer& incoming, MonoCanvas& output,
                    float progress) noexcept {
  if (endpoint(outgoing, incoming, output, progress)) return;
  const std::uint8_t coverage = static_cast<std::uint8_t>(
      normalized(progress) * 64.0F);
  dissolvePacked(outgoing, incoming, output, coverage);
}

float outQuad(float progress) noexcept {
  const float remaining = 1.0F - normalized(progress);
  return 1.0F - remaining * remaining;
}

float inOutQuad(float progress) noexcept {
  const float p = normalized(progress);
  return p < 0.5F ? 2.0F * p * p
                  : 1.0F - ((-2.0F * p + 2.0F) *
                            (-2.0F * p + 2.0F)) / 2.0F;
}
}  // namespace

void CutTransition::compose(const MonoFramebuffer& outgoing,
                            const MonoFramebuffer& incoming,
                            MonoCanvas& output, float progress) const noexcept {
  copyFrame(progress < 0.5F ? outgoing : incoming, output);
}

void VenetianBlindsTransition::compose(
    const MonoFramebuffer& outgoing, const MonoFramebuffer& incoming,
    MonoCanvas& output, float progress) const noexcept {
  const float normalized = std::max(0.0F, std::min(1.0F, progress));
  const MonoFramebuffer& base = normalized < 0.5F ? outgoing : incoming;
  copyFrame(base, output);
  if (normalized <= 0.0F || normalized >= 1.0F) return;

  const float shutter = normalized < 0.5F ? normalized * 2.0F
                                          : (1.0F - normalized) * 2.0F;
  for (std::int32_t blade = 0; blade < bladeCount_; ++blade) {
    const std::int32_t sectionX = blade * output.width() / bladeCount_;
    const std::int32_t sectionRight =
        (blade + 1) * output.width() / bladeCount_;
    const std::int32_t sectionWidth = sectionRight - sectionX;
    const std::int32_t bladeWidth = static_cast<std::int32_t>(
        (static_cast<float>(sectionWidth) + 1.0F) * shutter);
    const std::int32_t left = (blade & 1) == 0
                                  ? sectionX
                                  : sectionX + sectionWidth - bladeWidth;
    output.fillRect(left, 0, bladeWidth, output.height(), true);
  }
}

void DipTransition::compose(const MonoFramebuffer& outgoing,
                            const MonoFramebuffer& incoming,
                            MonoCanvas& output, float progress) const noexcept {
  if (endpoint(outgoing, incoming, output, progress)) return;
  dipPacked(outgoing, incoming, output, progress);
}

void HorizontalWipeTransition::compose(
    const MonoFramebuffer& outgoing, const MonoFramebuffer& incoming,
    MonoCanvas& output, float progress) const noexcept {
  if (endpoint(outgoing, incoming, output, progress)) return;
  const std::int32_t edge =
      static_cast<std::int32_t>(normalized(progress) * output.width());
  selectPixelsPacked(outgoing, incoming, output,
                     [edge](std::int32_t x, std::int32_t) { return x < edge; });
}

void DiagonalWipeTransition::compose(
    const MonoFramebuffer& outgoing, const MonoFramebuffer& incoming,
    MonoCanvas& output, float progress) const noexcept {
  if (endpoint(outgoing, incoming, output, progress)) return;
  const float edge = normalized(progress) *
                     static_cast<float>(output.width() + output.height() - 2);
  selectPixelsPacked(outgoing, incoming, output,
                     [edge](std::int32_t x, std::int32_t y) {
                       return static_cast<float>(x + y) <= edge;
                     });
}

void IrisTransition::compose(const MonoFramebuffer& outgoing,
                             const MonoFramebuffer& incoming,
                             MonoCanvas& output,
                             float progress) const noexcept {
  if (endpoint(outgoing, incoming, output, progress)) return;
  const float centerX = (output.width() - 1) * 0.5F;
  const float centerY = (output.height() - 1) * 0.5F;
  const float maximum = std::sqrt(centerX * centerX + centerY * centerY);
  const float radius = normalized(progress) * maximum;
  const float radiusSquared = radius * radius;
  selectPixelsPacked(outgoing, incoming, output,
                     [centerX, centerY, radiusSquared](std::int32_t x,
                                                       std::int32_t y) {
                       const float dx = x - centerX;
                       const float dy = y - centerY;
                       return dx * dx + dy * dy <= radiusSquared;
                     });
}

void CheckerDissolveTransition::compose(
    const MonoFramebuffer& outgoing, const MonoFramebuffer& incoming,
    MonoCanvas& output, float progress) const noexcept {
  if (endpoint(outgoing, incoming, output, progress)) return;
  const std::uint8_t coverage =
      static_cast<std::uint8_t>(normalized(progress) * 64.0F);
  dissolvePacked(outgoing, incoming, output, coverage);
}

void AppLaunchHandoffTransition::compose(
    const MonoFramebuffer& outgoing, const MonoFramebuffer& incoming,
    MonoCanvas& output, float progress) const noexcept {
  if (progress <= 0.0F) {
    copyFrame(outgoing, output);
    return;
  }
  if (progress >= 1.0F) {
    copyFrame(incoming, output);
    return;
  }
  const float phase = progress < 0.5F ? progress * 2.0F
                                      : (progress - 0.5F) * 2.0F;
  dissolveFrames(outgoing, incoming, output,
                 progress < 0.5F ? outQuad(phase)
                                 : inOutQuad(phase));
}

void AppReturnHandoffTransition::compose(
    const MonoFramebuffer& outgoing, const MonoFramebuffer& incoming,
    MonoCanvas& output, float progress) const noexcept {
  if (progress <= 0.0F) {
    copyFrame(outgoing, output);
    return;
  }
  if (progress >= 1.0F) {
    copyFrame(incoming, output);
    return;
  }
  const float phase = progress < 0.5F ? progress * 2.0F
                                      : (progress - 0.5F) * 2.0F;
  // Keep the App-to-Cover phase perceptually even. An in/out curve bunches
  // ordered-dither thresholds into the middle frames and reads as a flash at
  // 30 FPS. Launcher chrome can still ease in after the shared Cover locks.
  dissolveFrames(outgoing, incoming, output,
                 progress < 0.5F ? phase : inOutQuad(phase));
}

}  // namespace cadenza
