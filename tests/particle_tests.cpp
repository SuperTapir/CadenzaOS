#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cmath>

#include "cadenza/presentation/particles.h"

namespace {
class ParticleSink final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    last = event;
    ++count;
  }
  cadenza::DiagnosticEvent last{};
  int count = 0;
};
}

TEST_CASE("particles spawn update accelerate and expire") {
  cadenza::ParticlePool<2> pool;
  cadenza::Particle particle;
  particle.position = {1.0F, 2.0F};
  particle.velocity = {4.0F, 0.0F};
  particle.acceleration = {0.0F, 2.0F};
  particle.lifetime = 1.0F;
  REQUIRE(pool.spawn(particle));
  pool.update(0.5F);
  REQUIRE(pool.particle(0) != nullptr);
  CHECK(std::abs(pool.particle(0)->position.x - 3.0F) < 0.00001F);
  CHECK(std::abs(pool.particle(0)->velocity.y - 1.0F) < 0.00001F);
  pool.update(1.0F);
  CHECK(pool.activeCount() == 0);
}

TEST_CASE("seeded emitters reproduce identical bursts") {
  cadenza::ParticlePool<4> first;
  cadenza::ParticlePool<4> second;
  cadenza::Particle base;
  base.lifetime = 2.0F;
  cadenza::ParticleEmitter firstEmitter{77};
  cadenza::ParticleEmitter secondEmitter{77};
  CHECK(firstEmitter.emit(first, base, 4, 3.0F) == 4);
  CHECK(secondEmitter.emit(second, base, 4, 3.0F) == 4);
  for (std::size_t index = 0; index < 4; ++index) {
    REQUIRE(first.particle(index) != nullptr);
    REQUIRE(second.particle(index) != nullptr);
    CHECK(first.particle(index)->velocity.x == second.particle(index)->velocity.x);
    CHECK(first.particle(index)->velocity.y == second.particle(index)->velocity.y);
  }
}

TEST_CASE("full pool reject and replace-oldest policies are explicit") {
  ParticleSink diagnostics;
  cadenza::Particle particle;
  particle.lifetime = 10.0F;
  cadenza::ParticlePool<2> reject{cadenza::ParticleFullPolicy::Reject,
                                  &diagnostics};
  REQUIRE(reject.spawn(particle));
  particle.position.x = 1.0F;
  REQUIRE(reject.spawn(particle));
  particle.position.x = 2.0F;
  CHECK_FALSE(reject.spawn(particle));
  CHECK(reject.activeCount() == 2);
  CHECK(diagnostics.last.code == cadenza::DiagnosticCode::CapacityExceeded);

  cadenza::ParticlePool<2> replace{cadenza::ParticleFullPolicy::ReplaceOldest};
  particle.position.x = 0.0F;
  REQUIRE(replace.spawn(particle));
  particle.position.x = 1.0F;
  REQUIRE(replace.spawn(particle));
  particle.position.x = 2.0F;
  REQUIRE(replace.spawn(particle));
  CHECK(replace.particle(0)->position.x == 2.0F);
}

TEST_CASE("particle renderer supports primitive and bitmap visuals") {
  const std::uint8_t spritePixels[] = {0x80};
  cadenza::ParticlePool<2> pool;
  cadenza::Particle pixel;
  pixel.position = {2.0F, 3.0F};
  pixel.lifetime = 1.0F;
  REQUIRE(pool.spawn(pixel));
  cadenza::Particle sprite;
  sprite.position = {5.0F, 6.0F};
  sprite.lifetime = 1.0F;
  sprite.visual = cadenza::ParticleVisual::Sprite;
  sprite.sprite = {spritePixels, 1, 1, 1};
  sprite.spriteSource = {0, 0, 1, 1};
  REQUIRE(pool.spawn(sprite));
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  pool.render(canvas);
  CHECK(framebuffer.pixel(2, 3));
  CHECK(framebuffer.pixel(5, 6));
}
