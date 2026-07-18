#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>

#include "cadenza/animation/composition.h"
#include "cadenza/animation/tween.h"

TEST_CASE("Sequence maps global time to consecutive child local times") {
  cadenza::Tween<float> first{0.0F, 10.0F, 1.0F};
  cadenza::Tween<float> second{10.0F, 20.0F, 2.0F};
  cadenza::Sequence<2> sequence;
  REQUIRE(sequence.add(first));
  REQUIRE(sequence.add(second));
  CHECK(sequence.duration() == 3.0F);
  sequence.seekTime(0.5F);
  CHECK(first.value() == 5.0F);
  CHECK(second.value() == 10.0F);
  sequence.seekTime(1.5F);
  CHECK(first.value() == 10.0F);
  CHECK(std::abs(second.value() - 12.5F) < 0.00001F);
  sequence.reset();
  sequence.update(3.0F);
  CHECK(second.value() == 20.0F);
  CHECK_FALSE(sequence.add(first));
}

TEST_CASE("Parallel duration is max child duration and local time is shared") {
  cadenza::Tween<float> shortTween{0.0F, 10.0F, 1.0F};
  cadenza::Tween<float> longTween{10.0F, 20.0F, 2.0F};
  cadenza::Parallel<2> parallel;
  REQUIRE(parallel.add(shortTween));
  REQUIRE(parallel.add(longTween));
  CHECK(parallel.duration() == 2.0F);
  parallel.seekTime(0.5F);
  CHECK(shortTween.value() == 5.0F);
  CHECK(std::abs(longTween.value() - 12.5F) < 0.00001F);
  parallel.seekTime(1.5F);
  CHECK(shortTween.value() == 10.0F);
  CHECK(std::abs(longTween.value() - 17.5F) < 0.00001F);
  CHECK_FALSE(parallel.add(shortTween));
}

TEST_CASE("Timeline supports overlap offsets seek reverse repeat and capacity") {
  cadenza::Tween<float> first{0.0F, 10.0F, 1.0F};
  cadenza::Tween<float> second{10.0F, 20.0F, 1.0F};
  cadenza::Timeline<2> timeline;
  REQUIRE(timeline.add(first, 0.0F));
  REQUIRE(timeline.add(second, 0.5F));
  CHECK(timeline.duration() == 1.5F);
  timeline.seekTime(0.75F);
  CHECK(first.value() == 7.5F);
  CHECK(second.value() == 12.5F);
  CHECK_FALSE(timeline.add(first, 2.0F));

  timeline.setReversed(true).setRepeat(1).reset();
  CHECK(timeline.playhead() == 1.5F);
  timeline.update(0.25F);
  CHECK(std::abs(timeline.playhead() - 1.25F) < 0.00001F);
  timeline.update(1.5F);
  CHECK(std::abs(timeline.playhead() - 1.25F) < 0.00001F);
  timeline.update(2.0F);
  CHECK(timeline.playhead() == 0.0F);
  CHECK(timeline.completed());
}
