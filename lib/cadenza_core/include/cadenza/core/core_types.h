#pragma once

#include <cstdint>

namespace cadenza {

using Seconds = float;
using FrameIndex = std::uint64_t;

enum class MotionProfile : std::uint8_t { Normal, Reduced };

}  // namespace cadenza
