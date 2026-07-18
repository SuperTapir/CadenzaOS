#pragma once

#include <cstdint>
#include <string>

#include "cadenza/core/core_types.h"
#include "cadenza/core/mono_framebuffer.h"

namespace cadenza::desktop {

std::string screenshotFilename(std::int16_t width, std::int16_t height,
                               FrameIndex frame,
                               std::uint64_t simulationMilliseconds);
bool writePng(const std::string& path,
              const MonoFramebuffer& framebuffer) noexcept;

}  // namespace cadenza::desktop
