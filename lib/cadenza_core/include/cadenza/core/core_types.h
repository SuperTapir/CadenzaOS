#pragma once

#include <cstdint>

namespace cadenza {

using Seconds = float;
using FrameIndex = std::uint64_t;
using CompletionCallback = void (*)(void*) noexcept;

enum class MotionProfile : std::uint8_t { Normal, Reduced };
enum class LauncherOrientation : std::uint8_t { Vertical, Horizontal };

}  // namespace cadenza
