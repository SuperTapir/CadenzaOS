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
                                         Seconds fixedDelta,
                                         system::SystemServiceHost* services) noexcept
    : runtime_(runtime),
      canvas_(canvas),
      framebuffer_(framebuffer),
      services_(services),
      fixedDelta_(fixedDelta > 0.0F ? fixedDelta : 1.0F / 60.0F) {}

void DeterministicRunner::render() noexcept {
  if (services_) {
    runtime_.renderWithSystem(canvas_, services_->snapshot());
  } else {
    runtime_.render(canvas_);
  }
}

void DeterministicRunner::step(const InputFrame& input) noexcept {
  advance(fixedDelta_, input);
}

void DeterministicRunner::advance(Seconds delta,
                                  const InputFrame& input) noexcept {
  if (delta < 0.0F) delta = 0.0F;
  const double elapsedMs = static_cast<double>(delta) * 1000.0 +
                           simulationSubmillis_;
  const auto wholeMs = static_cast<MonotonicMillis>(elapsedMs);
  simulationSubmillis_ = elapsedMs - static_cast<double>(wholeMs);
  simulationNowMs_ += wholeMs;
  advanceAt(simulationNowMs_, delta, input);
}

void DeterministicRunner::advanceAt(MonotonicMillis nowMs, Seconds delta,
                                    const InputFrame& input) noexcept {
  if (delta < 0.0F) delta = 0.0F;
  if (nowMs >= simulationNowMs_) simulationNowMs_ = nowMs;
  if (services_) {
    system::FrameCoordinator::runFrameAt(*services_, runtime_, canvas_, nowMs,
                                         delta, input);
  } else {
    runtime_.update(delta, input);
    render();
  }
  simulationSeconds_ += delta;
  ++frameIndex_;
}

HeadlessHost::HeadlessHost(FramebufferProfile profile,
                           Seconds fixedDelta,
                           DiagnosticSink* diagnostics) noexcept
    : connectivity_(services_),
      microphone_(services_),
      runtime_(profile),
      framebuffer_(profile),
      canvas_(framebuffer_, diagnostics),
      runner_(runtime_, canvas_, framebuffer_, fixedDelta, &services_) {
  runtime_.setDiagnosticSink(diagnostics);
  services_.setDiagnosticSink(diagnostics);
  runtime_.registerApp(apps::kLauncherAppId, launcher_, false,
                       apps::builtinAppCapabilities(apps::kLauncherAppId));
  runtime_.registerApp(apps::kTimerAppId, timer_, true,
                       apps::builtinAppCapabilities(apps::kTimerAppId));
  runtime_.registerApp(apps::kMotionAppId, motion_, true,
                       apps::builtinAppCapabilities(apps::kMotionAppId));
  runtime_.registerApp(apps::kSettingsAppId, settings_, true,
                       apps::builtinAppCapabilities(apps::kSettingsAppId));
  runtime_.registerApp(apps::kGalleryAppId, gallery_, true,
                       apps::builtinAppCapabilities(apps::kGalleryAppId));
  runtime_.configureHome(apps::kLauncherAppId);
  runtime_.begin(apps::kLauncherAppId);
  runtime_.bindSystem(services_.snapshot(), services_);
  runner_.render();
}

}  // namespace cadenza::host
