#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>

#include "cadenza/core/input.h"
#include "cadenza/desktop/simulation_controller.h"

TEST_CASE("simulation controller switches fixed and real step modes") {
  cadenza::desktop::SimulationController controller{0.02F};
  CHECK(std::abs(controller.consumeDelta(0.123F) - 0.02F) < 0.000001F);
  controller.setStepMode(cadenza::desktop::StepMode::Real);
  CHECK(std::abs(controller.consumeDelta(0.123F) - 0.123F) < 0.000001F);
}

TEST_CASE("simulation speed accepts only the four supported multipliers") {
  cadenza::desktop::SimulationController controller{0.02F};
  for (const float speed : {0.25F, 0.5F, 1.0F, 2.0F}) {
    REQUIRE(controller.setSpeed(speed));
    CHECK(std::abs(controller.consumeDelta(1.0F) - 0.02F * speed) < 0.000001F);
  }
  CHECK_FALSE(controller.setSpeed(0.75F));
  CHECK(controller.speed() == 2.0F);
}

TEST_CASE("pause resume and single step produce exact deltas") {
  cadenza::desktop::SimulationController controller{1.0F / 60.0F};
  controller.pause();
  CHECK(controller.paused());
  CHECK(controller.consumeDelta(0.5F) == 0.0F);
  controller.requestSingleStep();
  CHECK(std::abs(controller.consumeDelta(0.5F) - 1.0F / 60.0F) < 0.000001F);
  CHECK(controller.consumeDelta(0.5F) == 0.0F);
  controller.resume();
  CHECK_FALSE(controller.paused());
  CHECK(controller.consumeDelta(0.5F) > 0.0F);
}

TEST_CASE("wall-time input reduction continues while simulation is paused") {
  cadenza::desktop::SimulationController controller;
  cadenza::InputReducer reducer{{0, 650}};
  controller.pause();
  reducer.push({cadenza::RawInputType::ButtonDown, 100, 0});
  reducer.advanceTo(800);
  const cadenza::InputFrame input = reducer.takeFrame();
  CHECK(controller.consumeDelta(0.7F) == 0.0F);
  CHECK(input.pressed);
  CHECK(input.longPressed);
  CHECK(input.heldMs == 700);
}
