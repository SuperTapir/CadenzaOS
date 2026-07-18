#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>

#include "cadenza/animation/tween.h"

namespace {
void increment(void* context) noexcept { ++*static_cast<int*>(context); }
}

TEST_CASE("typed Tween updates value progress reset and state") {
  cadenza::Tween<float> tween{10.0F, 20.0F, 1.0F, cadenza::Easing::Linear};
  CHECK(tween.state() == cadenza::TweenState::Running);
  CHECK(tween.value() == 10.0F);
  tween.update(0.5F);
  CHECK(tween.value() == 15.0F);
  CHECK(tween.progress() == 0.5F);
  tween.update(0.5F);
  CHECK(tween.value() == 20.0F);
  CHECK(tween.state() == cadenza::TweenState::Completed);
  tween.reset();
  CHECK(tween.value() == 10.0F);
  CHECK(tween.state() == cadenza::TweenState::Running);

  cadenza::Tween<int> integer{0, 10, 1.0F};
  integer.update(0.5F);
  CHECK(integer.value() == 5);
}

TEST_CASE("Tween delay and large delta cross boundaries exactly") {
  cadenza::Tween<float> tween{0.0F, 1.0F, 1.0F};
  tween.setDelay(0.2F);
  CHECK(tween.state() == cadenza::TweenState::Delayed);
  tween.update(0.1F);
  CHECK(tween.value() == 0.0F);
  tween.update(0.6F);
  CHECK(std::abs(tween.value() - 0.5F) < 0.000001F);
  tween.update(5.0F);
  CHECK(tween.value() == 1.0F);
  CHECK(tween.state() == cadenza::TweenState::Completed);
}

TEST_CASE("Tween repeat delay yoyo and reverse preserve traversal semantics") {
  cadenza::Tween<float> tween{0.0F, 10.0F, 1.0F};
  tween.setRepeat(1, 0.25F).setYoyo(true);
  tween.update(1.1F);
  CHECK(tween.value() == 10.0F);
  CHECK(tween.state() == cadenza::TweenState::Delayed);
  tween.update(0.65F);
  CHECK(std::abs(tween.value() - 5.0F) < 0.00001F);
  tween.update(0.5F);
  CHECK(tween.value() == 0.0F);
  CHECK(tween.state() == cadenza::TweenState::Completed);

  cadenza::Tween<float> reversed{0.0F, 10.0F, 1.0F};
  reversed.setReversed(true);
  CHECK(reversed.value() == 10.0F);
  reversed.update(0.25F);
  CHECK(reversed.value() == 7.5F);
}

TEST_CASE("completion callback fires once and seek has no side effects") {
  int callbacks = 0;
  cadenza::Tween<float> tween{0.0F, 1.0F, 1.0F};
  tween.onComplete(increment, &callbacks);
  tween.seek(0.75F);
  CHECK(tween.value() == 0.75F);
  CHECK(callbacks == 0);
  tween.update(0.25F);
  CHECK(callbacks == 1);
  tween.update(10.0F);
  CHECK(callbacks == 1);
  tween.seek(0.25F);
  CHECK(tween.value() == 0.25F);
  CHECK(tween.state() == cadenza::TweenState::Completed);
  CHECK(callbacks == 1);
}
