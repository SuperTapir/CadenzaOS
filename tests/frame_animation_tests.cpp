#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "cadenza/animation/frame_animation.h"

namespace {

constexpr std::uint8_t kPixels[] = {0xF0, 0x00};

cadenza::SpriteAtlas<4> makeAtlas() {
  cadenza::SpriteAtlas<4> atlas{{kPixels, 8, 2, 1}};
  CHECK(atlas.add("idle-0", {0, 0, 2, 1}));
  CHECK(atlas.add("idle-1", {2, 0, 2, 1}));
  CHECK(atlas.add("run-0", {4, 0, 2, 1}));
  CHECK(atlas.add("run-1", {6, 0, 2, 1}));
  return atlas;
}

}  // namespace

TEST_CASE("once frame animation clamps and completes after large deltas") {
  auto atlas = makeAtlas();
  cadenza::FrameAnimation animation{atlas, 1, 3, 0.1F,
                                    cadenza::FramePlayback::Once};
  REQUIRE(animation.valid());
  CHECK(animation.frameIndex() == 1);
  animation.update(0.1F);
  CHECK(animation.frameIndex() == 2);
  animation.update(10.0F);
  CHECK(animation.frameIndex() == 3);
  CHECK(animation.completed());
  REQUIRE(animation.frame() != nullptr);
  CHECK(animation.frame()->name == atlas.frame(3)->name);
}

TEST_CASE("loop frame animation wraps without iterative stepping") {
  auto atlas = makeAtlas();
  cadenza::FrameAnimation animation{atlas, 0, 4, 0.25F,
                                    cadenza::FramePlayback::Loop};
  animation.update(1000.5F);
  CHECK(animation.frameIndex() == 2);
  CHECK_FALSE(animation.completed());
}

TEST_CASE("ping-pong visits endpoints once per traversal") {
  auto atlas = makeAtlas();
  cadenza::FrameAnimation animation{atlas, 0, 4, 0.1F,
                                    cadenza::FramePlayback::PingPong};
  const std::size_t expected[] = {1, 2, 3, 2, 1, 0};
  for (const auto frame : expected) {
    animation.update(0.1F);
    CHECK(animation.frameIndex() == frame);
  }
  animation.update(60.2F);
  CHECK(animation.frameIndex() == 2);
}

TEST_CASE("frame animation can be queried and sought without callbacks") {
  auto atlas = makeAtlas();
  cadenza::FrameAnimation animation{atlas, 1, 3, 0.2F,
                                    cadenza::FramePlayback::Once};
  animation.seek(1.0F);
  CHECK(animation.progress() == doctest::Approx(1.0F));
  CHECK(animation.frameIndex() == 3);
  CHECK_FALSE(animation.completed());
  animation.seek(-2.0F);
  CHECK(animation.frameIndex() == 1);
  animation.seek(0.5F);
  CHECK(animation.frameIndex() == 2);
  animation.reset();
  CHECK(animation.progress() == 0.0F);
  CHECK(animation.frameIndex() == 1);
}

TEST_CASE("invalid atlas ranges fail explicitly") {
  auto atlas = makeAtlas();
  cadenza::FrameAnimation empty{atlas, 0, 0, 0.1F};
  cadenza::FrameAnimation overrun{atlas, 3, 2, 0.1F};
  cadenza::FrameAnimation zeroDuration{atlas, 0, 1, 0.0F};
  CHECK_FALSE(empty.valid());
  CHECK_FALSE(overrun.valid());
  CHECK_FALSE(zeroDuration.valid());
  CHECK(empty.frame() == nullptr);
}
