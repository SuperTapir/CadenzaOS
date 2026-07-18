#include "cadenza/animation/composition.h"
#include "cadenza/animation/golden_vectors.h"
#include "cadenza/animation/spring.h"
#include "cadenza/animation/tween.h"

// PlatformIO compiles this translation unit with the ESP32 toolchain. The
// function need not run on boot; its purpose is to instantiate the same
// numerical templates and golden-vector calls used by host tests.
float cadenzaAnimationGoldenCompileProbe() {
  float checksum = 0.0F;
  for (const auto& vector : cadenza::kEasingGoldenVectors) {
    checksum += cadenza::ease(vector.easing, vector.progress);
  }
  cadenza::Tween<float> tween{0.0F, 1.0F, 1.0F};
  cadenza::Sequence<1> sequence;
  sequence.add(tween);
  sequence.seekTime(0.5F);
  cadenza::Spring spring;
  spring.setTarget(1.0F);
  spring.update(1.0F / 60.0F);
  return checksum + tween.value() + spring.value();
}
