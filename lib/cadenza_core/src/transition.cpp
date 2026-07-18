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

template <typename Selector>
void selectPixels(const MonoFramebuffer& outgoing,
                  const MonoFramebuffer& incoming, MonoCanvas& output,
                  Selector&& useIncoming) noexcept {
  output.resetClip();
  for (std::int32_t y = 0; y < output.height(); ++y) {
    for (std::int32_t x = 0; x < output.width(); ++x) {
      output.pixel(x, y, useIncoming(x, y) ? incoming.pixel(x, y)
                                            : outgoing.pixel(x, y));
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
  const float p = normalized(progress);
  output.resetClip();
  for (std::int32_t y = 0; y < output.height(); ++y) {
    for (std::int32_t x = 0; x < output.width(); ++x) {
      const std::uint8_t threshold =
          kOrderedDither8x8.thresholds[(y & 7) * 8 + (x & 7)];
      bool black = true;
      if (p < 0.5F) {
        const std::uint8_t coverage =
            static_cast<std::uint8_t>(p * 2.0F * 64.0F);
        black = outgoing.pixel(x, y) || threshold < coverage;
      } else if (p > 0.5F) {
        const std::uint8_t reveal =
            static_cast<std::uint8_t>((p - 0.5F) * 2.0F * 64.0F);
        black = threshold < reveal ? incoming.pixel(x, y) : true;
      }
      output.pixel(x, y, black);
    }
  }
}

void HorizontalWipeTransition::compose(
    const MonoFramebuffer& outgoing, const MonoFramebuffer& incoming,
    MonoCanvas& output, float progress) const noexcept {
  if (endpoint(outgoing, incoming, output, progress)) return;
  const std::int32_t edge =
      static_cast<std::int32_t>(normalized(progress) * output.width());
  selectPixels(outgoing, incoming, output,
               [edge](std::int32_t x, std::int32_t) { return x < edge; });
}

void DiagonalWipeTransition::compose(
    const MonoFramebuffer& outgoing, const MonoFramebuffer& incoming,
    MonoCanvas& output, float progress) const noexcept {
  if (endpoint(outgoing, incoming, output, progress)) return;
  const float edge = normalized(progress) *
                     static_cast<float>(output.width() + output.height() - 2);
  selectPixels(outgoing, incoming, output,
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
  selectPixels(outgoing, incoming, output,
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
  selectPixels(outgoing, incoming, output,
               [coverage](std::int32_t x, std::int32_t y) {
                 return kOrderedDither8x8
                            .thresholds[(y & 7) * 8 + (x & 7)] < coverage;
               });
}

}  // namespace cadenza
