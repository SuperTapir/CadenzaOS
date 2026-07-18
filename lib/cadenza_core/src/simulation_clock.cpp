#include "cadenza/core/simulation_clock.h"

#include <limits>

namespace cadenza {

SimulationFrame SimulationClock::advance(Seconds injectedDeltaSeconds) noexcept {
  if (!(injectedDeltaSeconds > 0.0F)) {
    return {0.0F, elapsedSeconds_, frameIndex_};
  }

  elapsedSeconds_ += injectedDeltaSeconds;
  if (frameIndex_ < std::numeric_limits<FrameIndex>::max()) {
    ++frameIndex_;
  }
  return {injectedDeltaSeconds, elapsedSeconds_, frameIndex_};
}

void SimulationClock::reset(Seconds elapsedSeconds) noexcept {
  elapsedSeconds_ = elapsedSeconds > 0.0F ? elapsedSeconds : 0.0F;
  frameIndex_ = 0;
}

}  // namespace cadenza
