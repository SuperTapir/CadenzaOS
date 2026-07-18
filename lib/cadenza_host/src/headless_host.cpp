#include "cadenza/host/headless_host.h"

namespace cadenza::host {

std::uint64_t framebufferHash(const MonoFramebuffer& framebuffer) noexcept {
  constexpr std::uint64_t kOffset = 14695981039346656037ULL;
  constexpr std::uint64_t kPrime = 1099511628211ULL;
  std::uint64_t hash = kOffset;
  const auto mix = [&](std::uint8_t value) {
    hash ^= value;
    hash *= kPrime;
  };
  mix(static_cast<std::uint8_t>(framebuffer.width() & 0xFF));
  mix(static_cast<std::uint8_t>((framebuffer.width() >> 8) & 0xFF));
  mix(static_cast<std::uint8_t>(framebuffer.height() & 0xFF));
  mix(static_cast<std::uint8_t>((framebuffer.height() >> 8) & 0xFF));
  for (std::size_t index = 0; index < framebuffer.sizeBytes(); ++index) {
    mix(framebuffer.data()[index]);
  }
  return hash;
}

std::uint64_t pcmHash(const std::int16_t* samples,
                      std::size_t count) noexcept {
  constexpr std::uint64_t kOffset = 14695981039346656037ULL;
  constexpr std::uint64_t kPrime = 1099511628211ULL;
  if (samples == nullptr) return kOffset;
  std::uint64_t hash = kOffset;
  for (std::size_t index = 0; index < count; ++index) {
    const std::uint16_t value = static_cast<std::uint16_t>(samples[index]);
    hash ^= static_cast<std::uint8_t>(value & 0xFFU);
    hash *= kPrime;
    hash ^= static_cast<std::uint8_t>(value >> 8U);
    hash *= kPrime;
  }
  return hash;
}

DeterministicRunner::DeterministicRunner(AppRuntime& runtime,
                                         MonoCanvas& canvas,
                                         MonoFramebuffer& framebuffer,
                                         Seconds fixedDelta) noexcept
    : runtime_(runtime),
      canvas_(canvas),
      framebuffer_(framebuffer),
      fixedDelta_(fixedDelta > 0.0F ? fixedDelta : 1.0F / 60.0F) {}

void DeterministicRunner::render() noexcept { runtime_.render(canvas_); }

void DeterministicRunner::step(const InputFrame& input) noexcept {
  advance(fixedDelta_, input);
}

void DeterministicRunner::advance(Seconds delta,
                                  const InputFrame& input) noexcept {
  if (delta < 0.0F) delta = 0.0F;
  runtime_.update(delta, input);
  render();
  simulationSeconds_ += delta;
  ++frameIndex_;
}

HeadlessHost::HeadlessHost(FramebufferProfile profile,
                           Seconds fixedDelta,
                           DiagnosticSink* diagnostics) noexcept
    : runtime_(profile),
      framebuffer_(profile),
      canvas_(framebuffer_, diagnostics),
      runner_(runtime_, canvas_, framebuffer_, fixedDelta) {
  runtime_.setDiagnosticSink(diagnostics);
  runtime_.registerApp(AppId::Launcher, launcher_, false);
  runtime_.registerApp(AppId::Clock, clock_);
  runtime_.registerApp(AppId::Motion, motion_);
  runtime_.registerApp(AppId::Settings, settings_);
  runtime_.registerApp(AppId::Gallery, gallery_);
  runtime_.begin(AppId::Launcher);
  runner_.render();
}

}  // namespace cadenza::host
