#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>

#include "cadenza/animation/composition.h"
#include "cadenza/animation/tween.h"

namespace {
void increment(void* context) noexcept { ++*static_cast<int*>(context); }

class CompositionSink final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    last = event;
    ++count;
  }
  cadenza::DiagnosticEvent last{};
  int count = 0;
};
}  // namespace

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

TEST_CASE("Timeline pause and normalized progress preserve the playhead") {
  cadenza::Tween<float> tween{0.0F, 1.0F, 2.0F};
  cadenza::Timeline<1> timeline;
  REQUIRE(timeline.add(tween, 0.0F));
  timeline.update(0.5F);
  CHECK(timeline.progress() == doctest::Approx(0.25F));
  timeline.pause();
  CHECK(timeline.paused());
  timeline.update(1.0F);
  CHECK(timeline.playhead() == doctest::Approx(0.5F));
  timeline.resume();
  timeline.update(0.5F);
  CHECK(timeline.progress() == doctest::Approx(0.5F));
  timeline.seek(0.75F);
  CHECK(timeline.playhead() == doctest::Approx(1.5F));
  CHECK(tween.value() == doctest::Approx(0.75F));
}

TEST_CASE("composition capacity failures emit inspectable diagnostics") {
  CompositionSink diagnostics;
  cadenza::Tween<float> tween{0.0F, 1.0F, 1.0F};
  cadenza::Timeline<1> timeline{&diagnostics};
  REQUIRE(timeline.add(tween, 0.0F));
  CHECK_FALSE(timeline.add(tween, 0.5F));
  CHECK(diagnostics.last.category == cadenza::DiagnosticCategory::Capacity);
  CHECK(diagnostics.last.code == cadenza::DiagnosticCode::CapacityExceeded);
  CHECK(timeline.size() == 1);
}

TEST_CASE("composite completion callbacks fire once and seek is side-effect free") {
  int sequenceCallbacks = 0;
  cadenza::Tween<float> sequenceTween{0.0F, 1.0F, 1.0F};
  cadenza::Sequence<1> sequence;
  REQUIRE(sequence.add(sequenceTween));
  sequence.onComplete(increment, &sequenceCallbacks);
  sequence.seekTime(1.0F);
  CHECK(sequenceCallbacks == 0);
  sequence.reset();
  sequence.update(1.0F);
  sequence.update(1.0F);
  CHECK(sequenceCallbacks == 1);

  int parallelCallbacks = 0;
  cadenza::Tween<float> parallelTween{0.0F, 1.0F, 1.0F};
  cadenza::Parallel<1> parallel;
  REQUIRE(parallel.add(parallelTween));
  parallel.onComplete(increment, &parallelCallbacks);
  parallel.update(2.0F);
  parallel.seekTime(0.0F);
  parallel.update(2.0F);
  CHECK(parallelCallbacks == 1);

  int timelineCallbacks = 0;
  cadenza::Tween<float> timelineTween{0.0F, 1.0F, 1.0F};
  cadenza::Timeline<1> timeline;
  REQUIRE(timeline.add(timelineTween, 0.0F));
  timeline.onComplete(increment, &timelineCallbacks);
  timeline.seek(1.0F);
  CHECK(timelineCallbacks == 0);
  timeline.reset();
  timeline.update(1.0F);
  timeline.update(1.0F);
  CHECK(timelineCallbacks == 1);
}
