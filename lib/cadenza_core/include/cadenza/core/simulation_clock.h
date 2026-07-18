#pragma once

#include "cadenza/core/core_types.h"

namespace cadenza {

struct SimulationFrame {
  Seconds deltaSeconds = 0.0F;
  Seconds elapsedSeconds = 0.0F;
  FrameIndex frameIndex = 0;
};

class SimulationClock {
 public:
  SimulationFrame advance(Seconds injectedDeltaSeconds) noexcept;
  void reset(Seconds elapsedSeconds = 0.0F) noexcept;

  Seconds elapsedSeconds() const noexcept { return elapsedSeconds_; }
  FrameIndex frameIndex() const noexcept { return frameIndex_; }

 private:
  Seconds elapsedSeconds_ = 0.0F;
  FrameIndex frameIndex_ = 0;
};

}  // namespace cadenza
