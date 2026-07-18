#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstddef>

#include "cadenza/core/sprite_atlas.h"

namespace {
class AtlasSink final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    last = event;
    ++count;
  }
  cadenza::DiagnosticEvent last{};
  int count = 0;
};
}  // namespace

TEST_CASE("sprite atlas stores fixed frame descriptors and supports lookup") {
  const std::uint8_t pixels[] = {0xA0, 0x50};
  const cadenza::BitmapView bitmap{pixels, 4, 2, 1};
  cadenza::SpriteAtlas<2> atlas{bitmap};

  CHECK(atlas.add("left", {0, 0, 2, 2}));
  CHECK(atlas.add("right", {2, 0, 2, 2}));
  REQUIRE(atlas.size() == 2);
  REQUIRE(atlas.frame(0) != nullptr);
  CHECK(atlas.frame(0)->source.x == 0);
  REQUIRE(atlas.find("right") != nullptr);
  CHECK(atlas.find("right")->source.x == 2);
}

TEST_CASE("sprite atlas capacity failure does not mutate existing frames") {
  const std::uint8_t pixels[] = {0xF0};
  AtlasSink diagnostics;
  cadenza::SpriteAtlas<1> atlas{{pixels, 4, 1, 1}, &diagnostics};
  REQUIRE(atlas.add("only", {0, 0, 2, 1}));
  CHECK_FALSE(atlas.add("overflow", {2, 0, 2, 1}));
  CHECK(atlas.size() == 1);
  CHECK(atlas.find("only") == atlas.frame(0));
  CHECK(diagnostics.last.code == cadenza::DiagnosticCode::CapacityExceeded);
}

TEST_CASE("sprite atlas rejects invalid, duplicate, and missing frames") {
  const std::uint8_t pixels[] = {0xF0};
  AtlasSink diagnostics;
  cadenza::SpriteAtlas<3> atlas{{pixels, 4, 1, 1}, &diagnostics};
  CHECK_FALSE(atlas.add(nullptr, {0, 0, 1, 1}));
  CHECK_FALSE(atlas.add("outside", {3, 0, 2, 1}));
  REQUIRE(atlas.add("ok", {0, 0, 1, 1}));
  CHECK_FALSE(atlas.add("ok", {1, 0, 1, 1}));
  CHECK(atlas.find("missing") == nullptr);
  CHECK(atlas.frame(99) == nullptr);
  CHECK(atlas.size() == 1);
  CHECK(diagnostics.count >= 5);
}
