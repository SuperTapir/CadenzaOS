#pragma once

#include "cadenza/core/core_types.h"

namespace cadenza::desktop {

enum class StepMode {
  Fixed,
  Real,
};

class SimulationController {
 public:
  explicit SimulationController(Seconds fixedDelta = 1.0F / 60.0F) noexcept;

  Seconds consumeDelta(Seconds realDelta) noexcept;
  void pause() noexcept { paused_ = true; }
  void resume() noexcept { paused_ = false; }
  void togglePause() noexcept { paused_ = !paused_; }
  void requestSingleStep() noexcept { singleStepPending_ = true; }
  bool setSpeed(float speed) noexcept;
  void setStepMode(StepMode mode) noexcept { mode_ = mode; }
  void toggleStepMode() noexcept {
    mode_ = mode_ == StepMode::Fixed ? StepMode::Real : StepMode::Fixed;
  }

  bool paused() const noexcept { return paused_; }
  float speed() const noexcept { return speed_; }
  Seconds fixedDelta() const noexcept { return fixedDelta_; }
  StepMode stepMode() const noexcept { return mode_; }

 private:
  Seconds fixedDelta_ = 1.0F / 60.0F;
  float speed_ = 1.0F;
  StepMode mode_ = StepMode::Fixed;
  bool paused_ = false;
  bool singleStepPending_ = false;
};

}  // namespace cadenza::desktop
