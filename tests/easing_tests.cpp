#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cmath>

#include "cadenza/animation/easing.h"
#include "cadenza/animation/golden_vectors.h"

TEST_CASE("every easing clamps input and has exact endpoints") {
  for (const auto easing : {
           cadenza::Easing::Linear, cadenza::Easing::InQuad,
           cadenza::Easing::OutQuad, cadenza::Easing::InOutQuad,
           cadenza::Easing::InOutCubic, cadenza::Easing::OutExpo,
           cadenza::Easing::OutBack, cadenza::Easing::OutBounce,
           cadenza::Easing::OutElastic}) {
    CHECK(cadenza::ease(easing, -1.0F) == 0.0F);
    CHECK(cadenza::ease(easing, 0.0F) == 0.0F);
    CHECK(cadenza::ease(easing, 1.0F) == 1.0F);
    CHECK(cadenza::ease(easing, 2.0F) == 1.0F);
  }
}

TEST_CASE("easing representative interior vectors are stable") {
  for (const auto& vector : cadenza::kEasingGoldenVectors) {
    CAPTURE(static_cast<int>(vector.easing));
    CHECK(std::abs(cadenza::ease(vector.easing, vector.progress) -
                   vector.expected) < 0.00001F);
  }
}
