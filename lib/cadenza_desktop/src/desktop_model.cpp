#include "cadenza/desktop/desktop_model.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace cadenza::desktop {

bool configure(DesktopConfig& output, const char* profile,
               int scale) noexcept {
  if (!profile || scale < 1 || scale > 4) return false;
  DesktopConfig candidate;
  if (std::strcmp(profile, "t-embed") == 0 || std::strcmp(profile, "320") == 0) {
    candidate.profile = FramebufferProfile::TEmbed;
    candidate.width = 320;
    candidate.height = 170;
  } else if (std::strcmp(profile, "sharp") == 0 ||
             std::strcmp(profile, "400") == 0) {
    candidate.profile = FramebufferProfile::Sharp;
    candidate.width = 400;
    candidate.height = 240;
  } else {
    return false;
  }
  candidate.scale = static_cast<std::uint8_t>(scale);
  output = candidate;
  return true;
}

bool parseDisplayPalette(DisplayPalette& output,
                         const char* value) noexcept {
  if (!value) return false;
  DisplayPalette candidate;
  if (std::strcmp(value, "reflective") == 0) {
    candidate = DisplayPalette::Reflective;
  } else if (std::strcmp(value, "pure") == 0) {
    candidate = DisplayPalette::Pure;
  } else {
    return false;
  }
  output = candidate;
  return true;
}

DisplayColor displayColor(bool black,
                          DisplayPalette palette) noexcept {
  if (palette == DisplayPalette::Pure) {
    return black ? DisplayColor{0, 0, 0, 255}
                 : DisplayColor{255, 255, 255, 255};
  }
  // Playdate SDK 1.12.3 design-reference simulator captures use this warm,
  // low-contrast ink/paper pair to approximate a well-lit reflective LCD.
  return black ? DisplayColor{50, 47, 40, 255}
               : DisplayColor{177, 174, 167, 255};
}

bool DesktopInputMapper::wheel(float deltaY,
                               MonotonicMillis timestampMs) noexcept {
  if (!std::isfinite(deltaY) || deltaY == 0.0F) return false;
  float magnitude = std::ceil(std::abs(deltaY));
  magnitude = std::min(magnitude,
                       static_cast<float>(std::numeric_limits<std::int32_t>::max()));
  const std::int32_t value = static_cast<std::int32_t>(magnitude) *
                             (deltaY > 0.0F ? 1 : -1);
  return enqueue(RawInputType::Turn, value, timestampMs);
}

bool DesktopInputMapper::key(DesktopKey keyValue, bool down, bool repeat,
                             MonotonicMillis timestampMs) noexcept {
  if (keyValue == DesktopKey::Left || keyValue == DesktopKey::Right) {
    if (!down) return false;
    return enqueue(RawInputType::Turn,
                   keyValue == DesktopKey::Left ? -1 : 1, timestampMs);
  }
  if (keyValue != DesktopKey::Space && keyValue != DesktopKey::Enter) {
    return false;
  }
  if (repeat) return false;

  const bool wasDown = anyButtonDown();
  bool& state = keyValue == DesktopKey::Space ? spaceDown_ : enterDown_;
  if (state == down) return false;
  state = down;
  const bool isDown = anyButtonDown();
  if (wasDown == isDown) return true;
  return enqueue(isDown ? RawInputType::ButtonDown : RawInputType::ButtonUp,
                 0, timestampMs);
}

bool DesktopInputMapper::poll(RawInputEvent& event) noexcept {
  if (read_ == write_) return false;
  event = queue_[read_];
  read_ = (read_ + 1) % kCapacity;
  return true;
}

bool DesktopInputMapper::enqueue(RawInputType type, std::int32_t value,
                                 MonotonicMillis timestampMs) noexcept {
  const std::size_t next = (write_ + 1) % kCapacity;
  if (next == read_) return false;
  queue_[write_] = {type, timestampMs, value};
  write_ = next;
  return true;
}

}  // namespace cadenza::desktop
