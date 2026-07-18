#include "cadenza/desktop/simulation_controller.h"

#include <algorithm>

namespace cadenza::desktop {

SimulationController::SimulationController(Seconds fixedDelta) noexcept
    : fixedDelta_(fixedDelta > 0.0F ? fixedDelta : 1.0F / 60.0F) {}

Seconds SimulationController::consumeDelta(Seconds realDelta) noexcept {
  if (singleStepPending_) {
    singleStepPending_ = false;
    return fixedDelta_;
  }
  if (paused_) return 0.0F;
  const Seconds base = mode_ == StepMode::Fixed
                           ? fixedDelta_
                           : std::max(0.0F, realDelta);
  return base * speed_;
}

bool SimulationController::setSpeed(float speed) noexcept {
  if (speed != 0.25F && speed != 0.5F && speed != 1.0F && speed != 2.0F) {
    return false;
  }
  speed_ = speed;
  return true;
}

}  // namespace cadenza::desktop
