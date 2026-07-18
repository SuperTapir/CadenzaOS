#include "cadenza/core/transition.h"

#include <algorithm>
#include <cstdint>

namespace cadenza {
namespace {
void copyFrame(const MonoFramebuffer& source, MonoCanvas& output) noexcept {
  output.resetClip();
  output.drawFramebuffer(source, {0, 0, source.width(), source.height()}, 0, 0);
}
}  // namespace

const CutTransition kCutTransition;
const VenetianBlindsTransition kVenetianBlindsTransition;

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
  const std::int32_t sectionWidth = output.width() / 8;
  const std::int32_t bladeWidth = static_cast<std::int32_t>(
      (static_cast<float>(sectionWidth) + 1.0F) * shutter);
  for (std::int32_t blade = 0; blade < 8; ++blade) {
    const std::int32_t sectionX = blade * output.width() / 8;
    const std::int32_t left = (blade & 1) == 0
                                  ? sectionX
                                  : sectionX + sectionWidth - bladeWidth;
    output.fillRect(left, 0, bladeWidth, output.height(), true);
  }
}

}  // namespace cadenza
