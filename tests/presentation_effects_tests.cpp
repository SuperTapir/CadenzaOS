#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cmath>

#include "cadenza/presentation/effects.h"

TEST_CASE("selection feedback retargets without jumps and remains bounded") {
  cadenza::SelectionFeedback feedback{cadenza::MotionProfile::Normal};
  feedback.trigger();
  const float before = feedback.scale();
  feedback.trigger();
  CHECK(feedback.scale() == before);
  float maximum = feedback.scale();
  for (int frame = 0; frame < 240; ++frame) {
    feedback.update(1.0F / 120.0F);
    maximum = std::max(maximum, feedback.scale());
  }
  CHECK(maximum > 1.01F);
  CHECK(maximum <= 1.20F);
  CHECK(feedback.scale() == 1.0F);
}

TEST_CASE("reduced motion preserves feedback with lower amplitude") {
  cadenza::SelectionFeedback normal{cadenza::MotionProfile::Normal};
  cadenza::SelectionFeedback reduced{cadenza::MotionProfile::Reduced};
  normal.trigger();
  reduced.trigger();
  float normalMaximum = 1.0F;
  float reducedMaximum = 1.0F;
  for (int frame = 0; frame < 120; ++frame) {
    normal.update(1.0F / 120.0F);
    reduced.update(1.0F / 120.0F);
    normalMaximum = std::max(normalMaximum, normal.scale());
    reducedMaximum = std::max(reducedMaximum, reduced.scale());
  }
  CHECK(reducedMaximum > 1.0F);
  CHECK(reducedMaximum < normalMaximum);
}

TEST_CASE("camera punch is bounded and returns exactly neutral") {
  cadenza::CameraPunch punch{8.0F, 0.2F, cadenza::MotionProfile::Normal};
  CHECK(punch.offset() == 0.0F);
  punch.trigger();
  punch.update(0.1F);
  CHECK(std::abs(punch.offset()) > 0.0F);
  CHECK(std::abs(punch.offset()) <= 8.0F);
  punch.update(1.0F);
  CHECK(punch.offset() == 0.0F);
  CHECK_FALSE(punch.active());
}

TEST_CASE("camera shake is seeded bounded decaying and neutral at end") {
  cadenza::CameraShake first{12345, 6.0F, 0.5F};
  cadenza::CameraShake second{12345, 6.0F, 0.5F};
  first.trigger();
  second.trigger();
  for (int frame = 0; frame < 20; ++frame) {
    first.update(0.025F);
    second.update(0.025F);
    CHECK(first.x() == second.x());
    CHECK(first.y() == second.y());
    CHECK(std::abs(first.x()) <= first.currentBound() + 0.00001F);
    CHECK(std::abs(first.y()) <= first.currentBound() + 0.00001F);
  }
  CHECK(first.x() == 0.0F);
  CHECK(first.y() == 0.0F);
  CHECK_FALSE(first.active());
}

TEST_CASE("zero simulation delta does not advance camera shake") {
  cadenza::CameraShake paused{12345, 6.0F, 0.5F};
  cadenza::CameraShake reference{12345, 6.0F, 0.5F};
  paused.trigger();
  reference.trigger();

  paused.update(0.0F);
  CHECK(paused.x() == 0.0F);
  CHECK(paused.y() == 0.0F);
  CHECK(paused.currentBound() == reference.currentBound());

  paused.update(0.1F);
  reference.update(0.1F);
  CHECK(paused.x() == reference.x());
  CHECK(paused.y() == reference.y());
}
