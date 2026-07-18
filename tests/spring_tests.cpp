#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>

#include "cadenza/animation/spring.h"

TEST_CASE("underdamped Spring overshoots then settles exactly") {
  cadenza::Spring spring{0.0F, {180.0F, 14.0F, 1.0F}};
  spring.setTarget(1.0F);
  float maximum = spring.value();
  for (int frame = 0; frame < 600; ++frame) {
    spring.update(1.0F / 120.0F);
    maximum = std::max(maximum, spring.value());
  }
  CHECK(maximum > 1.01F);
  CHECK(spring.settled());
  CHECK(spring.value() == 1.0F);
  CHECK(spring.velocity() == 0.0F);
}

TEST_CASE("Spring reset and retarget preserve explicit state") {
  cadenza::Spring spring{2.0F};
  spring.setTarget(5.0F);
  spring.update(0.1F);
  CHECK(spring.value() != 2.0F);
  spring.reset(-3.0F);
  CHECK(spring.value() == -3.0F);
  CHECK(spring.target() == -3.0F);
  CHECK(spring.velocity() == 0.0F);
}

TEST_CASE("large delta is bounded by fixed substep budget and stays finite") {
  cadenza::SpringConfig config;
  config.maxSubsteps = 32;
  config.fixedStep = 1.0F / 120.0F;
  cadenza::Spring spring{0.0F, config};
  spring.setTarget(1.0F);
  spring.update(10.0F);
  CHECK(spring.lastSubsteps() == 32);
  CHECK(std::isfinite(spring.value()));
  CHECK(std::isfinite(spring.velocity()));
  CHECK(std::abs(spring.value()) < 10.0F);
}
