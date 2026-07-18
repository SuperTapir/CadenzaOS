#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/diagnostics.h"
#include "cadenza/core/mono_canvas.h"

namespace cadenza {

struct ParticleVector {
  float x = 0.0F;
  float y = 0.0F;
};

enum class ParticleVisual : std::uint8_t { Pixel, Circle, Sprite };
enum class ParticleFullPolicy : std::uint8_t { Reject, ReplaceOldest };

struct Particle {
  ParticleVector position;
  ParticleVector velocity;
  ParticleVector acceleration;
  float lifetime = 1.0F;
  float age = 0.0F;
  ParticleVisual visual = ParticleVisual::Pixel;
  std::int32_t radius = 1;
  BitmapView sprite;
  Rect spriteSource;
};

template <std::size_t Capacity>
class ParticlePool {
 public:
  explicit ParticlePool(
      ParticleFullPolicy policy = ParticleFullPolicy::Reject,
      DiagnosticSink* diagnostics = nullptr) noexcept
      : policy_(policy), diagnostics_(diagnostics) {}

  bool spawn(const Particle& particle) noexcept {
    std::size_t slot = Capacity;
    for (std::size_t index = 0; index < Capacity; ++index) {
      if (!slots_[index].active) {
        slot = index;
        break;
      }
    }
    if (slot == Capacity && policy_ == ParticleFullPolicy::ReplaceOldest &&
        Capacity > 0) {
      slot = 0;
      for (std::size_t index = 1; index < Capacity; ++index) {
        if (slots_[index].serial < slots_[slot].serial) slot = index;
      }
    }
    if (slot == Capacity) {
      if (diagnostics_) {
        diagnostics_->emit({DiagnosticCategory::Capacity,
                            DiagnosticCode::CapacityExceeded,
                            "particle pool full",
                            static_cast<std::int32_t>(Capacity)});
      }
      return false;
    }
    slots_[slot].particle = particle;
    slots_[slot].particle.age = 0.0F;
    slots_[slot].serial = nextSerial_++;
    slots_[slot].active = particle.lifetime > 0.0F;
    return slots_[slot].active;
  }

  void update(float delta) noexcept {
    if (delta <= 0.0F) return;
    for (auto& slot : slots_) {
      if (!slot.active) continue;
      slot.particle.age += delta;
      if (slot.particle.age >= slot.particle.lifetime) {
        slot.active = false;
        continue;
      }
      slot.particle.velocity.x += slot.particle.acceleration.x * delta;
      slot.particle.velocity.y += slot.particle.acceleration.y * delta;
      slot.particle.position.x += slot.particle.velocity.x * delta;
      slot.particle.position.y += slot.particle.velocity.y * delta;
    }
  }

  void render(MonoCanvas& canvas) const noexcept {
    for (const auto& slot : slots_) {
      if (!slot.active) continue;
      const std::int32_t x = static_cast<std::int32_t>(
          std::lround(slot.particle.position.x));
      const std::int32_t y = static_cast<std::int32_t>(
          std::lround(slot.particle.position.y));
      switch (slot.particle.visual) {
        case ParticleVisual::Pixel:
          canvas.pixel(x, y);
          break;
        case ParticleVisual::Circle:
          canvas.fillCircle(x, y, slot.particle.radius);
          break;
        case ParticleVisual::Sprite:
          canvas.drawBitmap(slot.particle.sprite, slot.particle.spriteSource, x,
                            y, {BitmapComposition::SetBlack, false, false});
          break;
      }
    }
  }

  const Particle* particle(std::size_t index) const noexcept {
    return index < Capacity && slots_[index].active ? &slots_[index].particle
                                                     : nullptr;
  }
  std::size_t activeCount() const noexcept {
    std::size_t count = 0;
    for (const auto& slot : slots_) count += slot.active ? 1U : 0U;
    return count;
  }
  constexpr std::size_t capacity() const noexcept { return Capacity; }

 private:
  struct Slot {
    Particle particle;
    std::uint64_t serial = 0;
    bool active = false;
  };
  std::array<Slot, Capacity> slots_{};
  std::uint64_t nextSerial_ = 0;
  ParticleFullPolicy policy_ = ParticleFullPolicy::Reject;
  DiagnosticSink* diagnostics_ = nullptr;
};

class ParticleEmitter {
 public:
  explicit ParticleEmitter(std::uint32_t seed) noexcept
      : state_(seed == 0 ? 1U : seed) {}

  template <std::size_t Capacity>
  std::size_t emit(ParticlePool<Capacity>& pool, const Particle& base,
                   std::size_t count, float velocitySpread) noexcept {
    std::size_t emitted = 0;
    for (std::size_t index = 0; index < count; ++index) {
      Particle particle = base;
      particle.velocity.x += randomSigned() * velocitySpread;
      particle.velocity.y += randomSigned() * velocitySpread;
      emitted += pool.spawn(particle) ? 1U : 0U;
    }
    return emitted;
  }

 private:
  float randomSigned() noexcept {
    state_ ^= state_ << 13U;
    state_ ^= state_ >> 17U;
    state_ ^= state_ << 5U;
    return static_cast<float>(state_ & 0x00FFFFFFU) / 8388607.5F - 1.0F;
  }
  std::uint32_t state_ = 1;
};

}  // namespace cadenza
