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
  runtime_.update(fixedDelta_, input);
  render();
  simulationSeconds_ += fixedDelta_;
  ++frameIndex_;
}

HeadlessHost::HeadlessHost(FramebufferProfile profile,
                           Seconds fixedDelta) noexcept
    : runtime_(profile),
      framebuffer_(profile),
      canvas_(framebuffer_),
      runner_(runtime_, canvas_, framebuffer_, fixedDelta) {
  runtime_.registerApp(AppId::Launcher, launcher_, false);
  runtime_.registerApp(AppId::Clock, clock_);
  runtime_.registerApp(AppId::Motion, motion_);
  runtime_.registerApp(AppId::Settings, settings_);
  runtime_.begin(AppId::Launcher);
  runner_.render();
}

}  // namespace cadenza::host
