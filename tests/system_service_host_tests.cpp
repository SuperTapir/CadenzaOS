#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>

#include "cadenza/system/system_service_host.h"
#include "cadenza/system/frame_coordinator.h"

namespace {
class FixedCapabilityResolver final : public cadenza::AppCapabilityResolver {
 public:
  cadenza::AppId id{};
  cadenza::AppCapabilitySet capabilities{};
  bool resolveCapabilities(
      cadenza::AppId candidate,
      cadenza::AppCapabilitySet& result) const noexcept override {
    if (candidate != id) return false;
    result = capabilities;
    return true;
  }
};

class FrameProbeApp final : public cadenza::App {
 public:
  const char* name() const noexcept override { return "Probe"; }
  void update(const cadenza::AppUpdateContext& context) noexcept override {
    updateVolume = context.system.soundVolume;
    updateOrientation = context.system.launcherOrientation;
    context.commands.submit(cadenza::SystemCommand::setSoundVolume(
        cadenza::audio::SoundVolume::Low));
    context.commands.submit(cadenza::SystemCommand::setLauncherOrientation(
        cadenza::LauncherOrientation::Horizontal));
  }
  void render(cadenza::MonoCanvas&,
              const cadenza::AppRenderContext& context) noexcept override {
    renderVolume = context.system.soundVolume;
    renderOrientation = context.system.launcherOrientation;
  }
  cadenza::audio::SoundVolume updateVolume = cadenza::audio::SoundVolume::Count;
  cadenza::audio::SoundVolume renderVolume = cadenza::audio::SoundVolume::Count;
  cadenza::LauncherOrientation updateOrientation =
      cadenza::LauncherOrientation::Horizontal;
  cadenza::LauncherOrientation renderOrientation =
      cadenza::LauncherOrientation::Vertical;
};

class VoiceOwnerProbeApp final : public cadenza::App {
 public:
  const char* name() const noexcept override { return "VoiceOwner"; }
  void update(const cadenza::AppUpdateContext& context) noexcept override {
    context.commands.submit(
        cadenza::SystemCommand::setVoiceAnalyzerActive(true));
  }
  void render(cadenza::MonoCanvas&,
              const cadenza::AppRenderContext&) noexcept override {}
};

class NetworkOwnerProbeApp final : public cadenza::App {
 public:
  const char* name() const noexcept override { return "NetworkOwner"; }
  void update(const cadenza::AppUpdateContext& context) noexcept override {
    context.commands.submit(
        cadenza::SystemCommand::setNetworkOnlineRequested(true));
  }
  void render(cadenza::MonoCanvas&,
              const cadenza::AppRenderContext&) noexcept override {}
};

class PassiveProbeApp final : public cadenza::App {
 public:
  const char* name() const noexcept override { return "Passive"; }
  void update(const cadenza::AppUpdateContext&) noexcept override {}
  void render(cadenza::MonoCanvas&,
              const cadenza::AppRenderContext& context) noexcept override {
    renderMicrophoneInUse = context.system.voice.microphoneInUse;
  }
  bool renderMicrophoneInUse = false;
};
}  // namespace

TEST_CASE("frame coordinator exposes frozen update and committed render state") {
  cadenza::system::SystemServiceHost host;
  cadenza::AppRuntime runtime;
  FrameProbeApp app;
  constexpr cadenza::AppId kHome{0x7100};
  REQUIRE(runtime.registerApp(
      kHome, app, false,
      cadenza::AppCapabilitySet{cadenza::AppCapability::SettingsWrite}));
  REQUIRE(runtime.configureHome(kHome));
  REQUIRE(runtime.begin(kHome));
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};

  cadenza::system::FrameCoordinator::runFrame(host, runtime, canvas, 0.01F,
                                               {});
  CHECK(app.updateVolume == cadenza::audio::SoundVolume::Medium);
  CHECK(app.renderVolume == cadenza::audio::SoundVolume::Low);
  CHECK(app.updateOrientation == cadenza::LauncherOrientation::Vertical);
  CHECK(app.renderOrientation == cadenza::LauncherOrientation::Horizontal);
}

TEST_CASE("Launcher orientation defaults vertical and commits in frame order") {
  cadenza::system::SystemServiceHost host;
  const cadenza::SystemSnapshot& update = host.beginFrame(0.01F);
  CHECK(update.launcherOrientation == cadenza::LauncherOrientation::Vertical);

  REQUIRE(host.submit(cadenza::SystemCommand::setLauncherOrientation(
      cadenza::LauncherOrientation::Horizontal)));
  CHECK(update.launcherOrientation == cadenza::LauncherOrientation::Vertical);
  CHECK(host.commitCommands().launcherOrientation ==
        cadenza::LauncherOrientation::Horizontal);
}

TEST_CASE("Launcher orientation validates values and SettingsWrite capability") {
  cadenza::system::SystemServiceHost host;
  CHECK_FALSE(host.submit(cadenza::SystemCommand::setLauncherOrientation(
      static_cast<cadenza::LauncherOrientation>(99))));
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::InvalidCommand);

  constexpr cadenza::AppId kCaller{0x7410};
  FixedCapabilityResolver resolver;
  resolver.id = kCaller;
  host.bindCapabilityResolver(resolver);
  cadenza::AppCommandPort denied{kCaller, {}, host};
  CHECK_FALSE(denied.submit(cadenza::SystemCommand::setLauncherOrientation(
      cadenza::LauncherOrientation::Horizontal)));
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::CapabilityDenied);
  CHECK(host.snapshot().launcherOrientation ==
        cadenza::LauncherOrientation::Vertical);

  resolver.capabilities = cadenza::AppCapabilitySet{
      cadenza::AppCapability::SettingsWrite};
  cadenza::AppCommandPort authorized{kCaller, resolver.capabilities, host};
  REQUIRE(authorized.submit(cadenza::SystemCommand::setLauncherOrientation(
      cadenza::LauncherOrientation::Horizontal)));
  CHECK(host.commitCommands().launcherOrientation ==
        cadenza::LauncherOrientation::Horizontal);
}

TEST_CASE("frame freezes update snapshot and commits FIFO for render") {
  cadenza::system::SystemServiceHost host;
  const cadenza::SystemSnapshot& update = host.beginFrame(0.01F);
  CHECK(update.motionProfile == cadenza::MotionProfile::Normal);
  CHECK(update.soundVolume == cadenza::audio::SoundVolume::Medium);

  REQUIRE(host.submit(cadenza::SystemCommand::setMotionProfile(
      cadenza::MotionProfile::Reduced)));
  REQUIRE(host.submit(cadenza::SystemCommand::setSoundVolume(
      cadenza::audio::SoundVolume::Low)));
  REQUIRE(host.submit(cadenza::SystemCommand::setSoundVolume(
      cadenza::audio::SoundVolume::Medium)));

  CHECK(update.motionProfile == cadenza::MotionProfile::Normal);
  CHECK(update.soundVolume == cadenza::audio::SoundVolume::Medium);
  const cadenza::SystemSnapshot& render = host.commitCommands();
  CHECK(render.motionProfile == cadenza::MotionProfile::Reduced);
  CHECK(render.soundVolume == cadenza::audio::SoundVolume::Medium);
  CHECK(host.diagnostics().committedCommands == 3);
}

TEST_CASE("legacy command sink accepts an operation without caller identity") {
  cadenza::system::SystemServiceHost host;
  REQUIRE(host.submit(cadenza::SystemCommand::setMotionProfile(
      cadenza::MotionProfile::Reduced)));
  CHECK(host.commitCommands().motionProfile ==
        cadenza::MotionProfile::Reduced);
  CHECK(host.diagnostics().committedCommands == 1);
}

TEST_CASE("operation envelope validates owner generation and system paths") {
  cadenza::system::SystemServiceHost host;
  CHECK_FALSE(host.submit({{}, cadenza::SystemCommand::setMotionProfile(
                                   cadenza::MotionProfile::Reduced), 0}));
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::InvalidCaller);
  CHECK_FALSE(host.submit({cadenza::ResourceOwner::system(),
                           cadenza::SystemCommand::setMotionProfile(
                               cadenza::MotionProfile::Reduced),
                           7}));
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::StaleGeneration);
  REQUIRE(host.submit({cadenza::ResourceOwner::usb(),
                       cadenza::SystemCommand::setMotionProfile(
                           cadenza::MotionProfile::Reduced),
                       0}));
  CHECK(host.commitCommands().motionProfile ==
        cadenza::MotionProfile::Reduced);
}

TEST_CASE("App command port defaults deny and host revalidates adapters") {
  cadenza::system::SystemServiceHost host;
  constexpr cadenza::AppId kCaller{0x7400};
  FixedCapabilityResolver resolver;
  resolver.id = kCaller;
  host.bindCapabilityResolver(resolver);

  cadenza::AppCommandPort denied{kCaller, {}, host};
  CHECK_FALSE(denied.submit(cadenza::SystemCommand::setMotionProfile(
      cadenza::MotionProfile::Reduced)));
  CHECK(host.pendingCommandCount() == 0);
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::CapabilityDenied);
  CHECK(host.diagnostics().lastRejectedOwner ==
        cadenza::ResourceOwner::app(kCaller));

  REQUIRE(host.submit({cadenza::ResourceOwner::app(kCaller),
                       cadenza::SystemCommand::setMotionProfile(
                           cadenza::MotionProfile::Reduced),
                       0}));
  CHECK(host.commitCommands().motionProfile ==
        cadenza::MotionProfile::Normal);
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::CapabilityDenied);

  resolver.capabilities = cadenza::AppCapabilitySet{
      cadenza::AppCapability::SettingsWrite};
  cadenza::AppCommandPort authorized{kCaller, resolver.capabilities, host};
  REQUIRE(authorized.submit(cadenza::SystemCommand::setMotionProfile(
      cadenza::MotionProfile::Reduced)));
  CHECK(host.commitCommands().motionProfile ==
        cadenza::MotionProfile::Reduced);
}

TEST_CASE("TimerControl is denied by default and revalidated at commit") {
  cadenza::system::SystemServiceHost host;
  constexpr cadenza::AppId kCaller{0x7500};
  FixedCapabilityResolver resolver;
  resolver.id = kCaller;
  host.bindCapabilityResolver(resolver);

  cadenza::AppCommandPort denied{kCaller, {}, host};
  CHECK_FALSE(denied.submit(cadenza::SystemCommand::startTimer(120000)));
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::CapabilityDenied);
  CHECK(host.pendingCommandCount() == 0);

  resolver.capabilities =
      cadenza::AppCapabilitySet{cadenza::AppCapability::TimerControl};
  cadenza::AppCommandPort authorized{kCaller, resolver.capabilities, host};
  REQUIRE(authorized.submit(cadenza::SystemCommand::startTimer(120000)));
  resolver.capabilities = {};
  host.commitCommands();
  CHECK(host.snapshot().timer.state == cadenza::TimerState::Ready);
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::CapabilityDenied);
}

TEST_CASE("Timer commands commit FIFO and preserve the update snapshot") {
  cadenza::system::SystemServiceHost host;
  constexpr cadenza::AppId kCaller{0x7501};
  FixedCapabilityResolver resolver;
  resolver.id = kCaller;
  resolver.capabilities =
      cadenza::AppCapabilitySet{cadenza::AppCapability::TimerControl};
  host.bindCapabilityResolver(resolver);
  cadenza::AppCommandPort commands{kCaller, resolver.capabilities, host};

  const auto& update = host.beginFrame(0.0F);
  REQUIRE(commands.submit(cadenza::SystemCommand::startTimer(120000)));
  REQUIRE(commands.submit(cadenza::SystemCommand::pauseTimer()));
  CHECK(update.timer.state == cadenza::TimerState::Ready);
  const auto& render = host.commitCommands();
  CHECK(render.timer.state == cadenza::TimerState::Paused);
  CHECK(render.timer.remainingMs == 120000);
  CHECK(render.timer.owner == kCaller);
  CHECK(host.diagnostics().committedCommands == 2);
}

TEST_CASE("Timer command validation and state rejection fail closed") {
  cadenza::system::SystemServiceHost host;
  constexpr cadenza::AppId kCaller{0x7502};
  FixedCapabilityResolver resolver;
  resolver.id = kCaller;
  resolver.capabilities =
      cadenza::AppCapabilitySet{cadenza::AppCapability::TimerControl};
  host.bindCapabilityResolver(resolver);
  cadenza::AppCommandPort commands{kCaller, resolver.capabilities, host};

  CHECK_FALSE(commands.submit(cadenza::SystemCommand::startTimer(59999)));
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::InvalidCommand);
  REQUIRE(commands.submit(cadenza::SystemCommand::resumeTimer()));
  host.commitCommands();
  CHECK(host.snapshot().timer.state == cadenza::TimerState::Ready);
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::InvalidState);
}

TEST_CASE("Only the system can acknowledge an expired Timer") {
  cadenza::system::SystemServiceHost host;
  constexpr cadenza::AppId kCaller{0x7503};
  FixedCapabilityResolver resolver;
  resolver.id = kCaller;
  resolver.capabilities =
      cadenza::AppCapabilitySet{cadenza::AppCapability::TimerControl};
  host.bindCapabilityResolver(resolver);
  cadenza::AppCommandPort commands{kCaller, resolver.capabilities, host};

  host.beginFrame(0.0F);
  REQUIRE(commands.submit(cadenza::SystemCommand::startTimer(60000)));
  host.commitCommands();
  host.beginFrame(60.0F);
  REQUIRE(host.snapshot().timer.state == cadenza::TimerState::Expired);

  REQUIRE(commands.submit(cadenza::SystemCommand::acknowledgeTimer()));
  host.commitCommands();
  CHECK(host.snapshot().timer.state == cadenza::TimerState::Expired);
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::InvalidCaller);
  REQUIRE(host.submit(cadenza::SystemCommand::acknowledgeTimer()));
  CHECK(host.commitCommands().timer.state == cadenza::TimerState::Ready);
}

TEST_CASE("Explicit monotonic time advances Timer when presentation delta is zero") {
  cadenza::system::SystemServiceHost host;
  constexpr cadenza::AppId kCaller{0x7504};
  FixedCapabilityResolver resolver;
  resolver.id = kCaller;
  resolver.capabilities =
      cadenza::AppCapabilitySet{cadenza::AppCapability::TimerControl};
  host.bindCapabilityResolver(resolver);
  cadenza::AppCommandPort commands{kCaller, resolver.capabilities, host};

  host.beginFrameAt(1000, 0.0F);
  REQUIRE(commands.submit(cadenza::SystemCommand::startTimer(60000)));
  host.commitCommands();
  CHECK(host.beginFrameAt(60999, 0.0F).timer.remainingMs == 1);
  CHECK(host.beginFrameAt(61000, 0.0F).timer.state ==
        cadenza::TimerState::Expired);
}

TEST_CASE("Frame coordinator keeps monotonic Timer independent of clamped dt") {
  cadenza::system::SystemServiceHost host;
  cadenza::AppRuntime runtime;
  FrameProbeApp app;
  constexpr cadenza::AppId kHome{0x7505};
  REQUIRE(runtime.registerApp(
      kHome, app, false,
      cadenza::AppCapabilitySet{cadenza::AppCapability::SettingsWrite} |
          cadenza::AppCapability::TimerControl));
  REQUIRE(runtime.configureHome(kHome));
  REQUIRE(runtime.begin(kHome));
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};

  cadenza::system::FrameCoordinator::runFrameAt(host, runtime, canvas, 5000,
                                                0.05F, {});
  REQUIRE(host.submit({cadenza::ResourceOwner::app(kHome),
                       cadenza::SystemCommand::startTimer(60000), 0}));
  host.commitCommands();
  cadenza::system::FrameCoordinator::runFrameAt(host, runtime, canvas, 65000,
                                                0.05F, {});
  CHECK(host.snapshot().timer.state == cadenza::TimerState::Expired);
}

TEST_CASE("Timer expiry emits one alert per bounded cadence and ack stops it") {
  cadenza::system::SystemServiceHost host;
  constexpr cadenza::AppId kCaller{0x7506};
  FixedCapabilityResolver resolver;
  resolver.id = kCaller;
  resolver.capabilities =
      cadenza::AppCapabilitySet{cadenza::AppCapability::TimerControl};
  host.bindCapabilityResolver(resolver);
  cadenza::AppCommandPort commands{kCaller, resolver.capabilities, host};

  host.beginFrameAt(1000, 0.0F);
  REQUIRE(commands.submit(cadenza::SystemCommand::startTimer(60000)));
  host.commitCommands();
  host.beginFrameAt(61000, 0.0F);
  CHECK(host.sound().lastAcceptedCue() ==
        cadenza::audio::SoundCue::TimerComplete);
  CHECK(host.sound().pendingCommandCount() == 1);

  host.beginFrameAt(62999, 0.0F);
  CHECK(host.sound().pendingCommandCount() == 1);
  host.beginFrameAt(63000, 0.0F);
  CHECK(host.sound().pendingCommandCount() == 2);

  host.beginFrameAt(61000 + 60 * 60000, 0.0F);
  CHECK(host.sound().pendingCommandCount() == 3);
  host.beginFrameAt(61000 + 60 * 60000 + 1, 0.0F);
  CHECK(host.sound().pendingCommandCount() == 3);

  REQUIRE(host.submit(cadenza::SystemCommand::acknowledgeTimer()));
  host.commitCommands();
  std::array<std::int16_t, 2048> samples{};
  samples.fill(1234);
  host.renderAudio(samples.data(), samples.size());
  CHECK(std::all_of(samples.begin(), samples.end(),
                    [](std::int16_t sample) { return sample == 0; }));
  CHECK(host.snapshot().timer.state == cadenza::TimerState::Ready);
}

TEST_CASE("Muted Timer expiry remains visually expired and renders silence") {
  cadenza::system::SystemServiceHost host;
  constexpr cadenza::AppId kCaller{0x7507};
  FixedCapabilityResolver resolver;
  resolver.id = kCaller;
  resolver.capabilities =
      cadenza::AppCapabilitySet{cadenza::AppCapability::TimerControl};
  host.bindCapabilityResolver(resolver);
  cadenza::AppCommandPort commands{kCaller, resolver.capabilities, host};

  REQUIRE(host.submit(cadenza::SystemCommand::setSoundVolume(
      cadenza::audio::SoundVolume::Muted)));
  host.commitCommands();
  std::array<std::int16_t, 64> samples{};
  host.renderAudio(samples.data(), samples.size());
  host.beginFrameAt(0, 0.0F);
  REQUIRE(commands.submit(cadenza::SystemCommand::startTimer(60000)));
  host.commitCommands();
  host.beginFrameAt(60000, 0.0F);
  CHECK(host.snapshot().timer.state == cadenza::TimerState::Expired);
  samples.fill(1234);
  host.renderAudio(samples.data(), samples.size());
  CHECK(std::all_of(samples.begin(), samples.end(),
                    [](std::int16_t sample) { return sample == 0; }));
}

TEST_CASE("foreground voice analyzer lease is released when owning App exits") {
  cadenza::system::SystemServiceHost host;
  REQUIRE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::microphoneAvailability(true)));
  host.beginFrame(0.0F);

  cadenza::AppRuntime runtime;
  VoiceOwnerProbeApp owner;
  PassiveProbeApp passive;
  constexpr cadenza::AppId kOwner{0x7300};
  constexpr cadenza::AppId kPassive{0x7301};
  REQUIRE(runtime.registerApp(
      kOwner, owner, false,
      cadenza::AppCapabilitySet{cadenza::AppCapability::VoiceAnalyzer}));
  REQUIRE(runtime.registerApp(kPassive, passive, false));
  REQUIRE(runtime.configureHome(kOwner));
  REQUIRE(runtime.begin(kOwner));

  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  cadenza::system::FrameCoordinator::runFrame(host, runtime, canvas, 0.01F,
                                               {});
  REQUIRE(host.snapshot().voice.analyzerActive);
  REQUIRE(runtime.open(kPassive));
  cadenza::system::FrameCoordinator::runFrame(host, runtime, canvas, 0.41F,
                                               {});
  REQUIRE(runtime.currentId() == kPassive);

  CHECK_FALSE(host.snapshot().voice.analyzerActive);
  CHECK_FALSE(host.snapshot().voice.microphoneInUse);
  CHECK(host.leases().diagnostics().autoReleased == 1);
  CHECK_FALSE(passive.renderMicrophoneInUse);
}

TEST_CASE("releasing an App foreground lease keeps another voice owner") {
  cadenza::system::SystemServiceHost host;
  REQUIRE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::microphoneAvailability(true)));
  host.beginFrame(0.0F);
  REQUIRE(host.submit(
      cadenza::SystemCommand::setVoiceAnalyzerActive(true)));
  host.commitCommands();

  constexpr cadenza::AppId kApp{0x7310};
  FixedCapabilityResolver resolver;
  resolver.id = kApp;
  resolver.capabilities = cadenza::AppCapabilitySet{
      cadenza::AppCapability::VoiceAnalyzer};
  host.bindCapabilityResolver(resolver);
  REQUIRE(host.submit({
      cadenza::ResourceOwner::app(kApp),
      cadenza::SystemCommand::setVoiceAnalyzerActive(true), 0}));
  host.commitCommands();
  CHECK(host.leases().ownerCount(
            cadenza::system::SystemResource::VoiceAnalyzer) == 2);

  CHECK(host.releaseForeground(cadenza::ResourceOwner::app(kApp)) == 1);
  CHECK(host.snapshot().voice.analyzerActive);
  CHECK(host.snapshot().voice.microphoneInUse);
  REQUIRE(host.submit(
      cadenza::SystemCommand::setVoiceAnalyzerActive(false)));
  host.commitCommands();
  CHECK_FALSE(host.snapshot().voice.analyzerActive);
  CHECK_FALSE(host.snapshot().voice.microphoneInUse);
}

TEST_CASE("App network lease drives idempotent radio actions and auto cleanup") {
  cadenza::system::SystemServiceHost host;
  host.connectivityService().setWiFiAvailable(true);
  cadenza::AppRuntime runtime;
  NetworkOwnerProbeApp owner;
  PassiveProbeApp passive;
  constexpr cadenza::AppId kOwner{0x7320};
  constexpr cadenza::AppId kPassive{0x7321};
  REQUIRE(runtime.registerApp(
      kOwner, owner, false,
      cadenza::AppCapabilitySet{cadenza::AppCapability::NetworkAcquire}));
  REQUIRE(runtime.registerApp(kPassive, passive, false));
  REQUIRE(runtime.configureHome(kOwner));
  REQUIRE(runtime.begin(kOwner));
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};

  cadenza::system::FrameCoordinator::runFrame(host, runtime, canvas, 0.01F,
                                               {});
  CHECK(host.leases().desired(cadenza::system::SystemResource::Network));
  cadenza::system::ConnectivityAction action;
  REQUIRE(host.connectivityService().tryPopAction(action));
  CHECK(action.type == cadenza::system::ConnectivityActionType::StartWiFiRadio);
  CHECK_FALSE(host.connectivityService().tryPopAction(action));

  REQUIRE(runtime.open(kPassive));
  cadenza::system::FrameCoordinator::runFrame(host, runtime, canvas, 0.41F,
                                               {});
  CHECK_FALSE(host.leases().desired(cadenza::system::SystemResource::Network));
  CHECK(host.connectivityService().snapshot().wifi.desired ==
        cadenza::WiFiDesiredPolicy::IdleAllowed);
  REQUIRE(host.connectivityService().tryPopAction(action));
  CHECK(action.type == cadenza::system::ConnectivityActionType::StopWiFiRadio);
}

TEST_CASE("provisioning commands enforce exclusive owner and release session lease") {
  cadenza::system::SystemServiceHost host;
  REQUIRE(host.submit(cadenza::SystemCommand::startProvisioning()));
  host.commitCommands();
  const std::uint32_t generation =
      host.connectivityService().snapshot().provisioning.generation;
  CHECK(host.connectivityService().snapshot().provisioning.state ==
        cadenza::ProvisioningState::Advertising);
  CHECK(host.leases().ownerCount(
            cadenza::system::SystemResource::ProvisioningSession) == 1);

  REQUIRE(host.submit({cadenza::ResourceOwner::usb(),
                       cadenza::SystemCommand::startProvisioning(), 0}));
  host.commitCommands();
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::ServiceBusy);
  REQUIRE(host.submit({cadenza::ResourceOwner::usb(),
                       cadenza::SystemCommand::cancelProvisioning(), 0}));
  host.commitCommands();
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::NotOwner);

  REQUIRE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::provisioningProgress(
          generation, cadenza::ProvisioningState::Failed,
          cadenza::ProvisioningFailure::AuthenticationFailed)));
  host.beginFrame(0.0F);
  CHECK(host.leases().ownerCount(
            cadenza::system::SystemResource::ProvisioningSession) == 0);
  CHECK(host.connectivityService().snapshot().provisioning.state ==
        cadenza::ProvisioningState::Stopping);
  REQUIRE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::provisioningStopped(generation)));
  CHECK(host.beginFrame(0.0F).connectivity.provisioning.state ==
        cadenza::ProvisioningState::Failed);
}

TEST_CASE("resource leases are bounded idempotent and owner aware") {
  using cadenza::ResourceOwner;
  using namespace cadenza::system;
  SystemResourceLeaseTable leases;
  constexpr cadenza::AppId kFirst{1};
  constexpr cadenza::AppId kSecond{2};

  CHECK(leases.acquire(ResourceOwner::app(kFirst), SystemResource::Network,
                       LeaseLifetime::Foreground) ==
        LeaseAcquireResult::Acquired);
  CHECK(leases.acquire(ResourceOwner::app(kFirst), SystemResource::Network,
                       LeaseLifetime::Foreground) ==
        LeaseAcquireResult::AlreadyHeld);
  CHECK(leases.acquire(ResourceOwner::app(kSecond), SystemResource::Network,
                       LeaseLifetime::Session) ==
        LeaseAcquireResult::Acquired);
  CHECK(leases.ownerCount(SystemResource::Network) == 2);
  CHECK(leases.release(ResourceOwner::app(cadenza::AppId{99}),
                       SystemResource::Network) ==
        LeaseReleaseResult::NotFound);
  CHECK(leases.releaseForeground(ResourceOwner::app(kFirst)) == 1);
  CHECK(leases.desired(SystemResource::Network));
  CHECK(leases.release(ResourceOwner::app(kSecond), SystemResource::Network) ==
        LeaseReleaseResult::Released);
  CHECK_FALSE(leases.desired(SystemResource::Network));
  CHECK(leases.acquire(ResourceOwner::app(kFirst),
                       SystemResource::BluetoothAdvertising,
                       LeaseLifetime::Persistent) ==
        LeaseAcquireResult::PersistentDenied);
  CHECK(leases.diagnostics().duplicateAcquires == 1);
  CHECK(leases.diagnostics().unknownReleases == 1);
  CHECK(leases.diagnostics().autoReleased == 1);
  CHECK(leases.diagnostics().highWater == 2);
}

TEST_CASE("resource lease table reports capacity and keeps other owners") {
  using namespace cadenza::system;
  SystemResourceLeaseTable leases;
  for (std::size_t index = 0; index < SystemResourceLeaseTable::kCapacity;
       ++index) {
    REQUIRE(leases.acquire(
                cadenza::ResourceOwner::app(
                    cadenza::AppId{static_cast<std::uint16_t>(index + 1)}),
                SystemResource::VoiceAnalyzer, LeaseLifetime::Foreground) ==
            LeaseAcquireResult::Acquired);
  }
  CHECK(leases.acquire(cadenza::ResourceOwner::system(),
                       SystemResource::VoiceAnalyzer,
                       LeaseLifetime::Persistent) ==
        LeaseAcquireResult::CapacityExceeded);
  CHECK(leases.size() == SystemResourceLeaseTable::kCapacity);
  CHECK(leases.diagnostics().capacityFailures == 1);
  CHECK(leases.diagnostics().highWater ==
        SystemResourceLeaseTable::kCapacity);
}

TEST_CASE("platform callback ingestion cannot reenter authoritative state") {
  cadenza::system::SystemServiceHost host;
  CHECK_FALSE(host.snapshot().soundOutputAvailable);
  REQUIRE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::soundOutputAvailability(true)));
  CHECK_FALSE(host.snapshot().soundOutputAvailable);
  CHECK(host.beginFrame(0.0F).soundOutputAvailable);
}

TEST_CASE("bounded queues expose stable rejection and high water") {
  cadenza::system::SystemServiceHost host{{2, 1, 1, 1}};
  CHECK_FALSE(host.submit(cadenza::SystemCommand::setSoundVolume(
      cadenza::audio::SoundVolume::Count)));
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::InvalidCommand);
  REQUIRE(host.submit(cadenza::SystemCommand::playSound(
      cadenza::audio::SoundCue::Confirm)));
  REQUIRE(host.submit(cadenza::SystemCommand::playSound(
      cadenza::audio::SoundCue::Back)));
  CHECK_FALSE(host.submit(cadenza::SystemCommand::playSound(
      cadenza::audio::SoundCue::Reject)));
  CHECK(host.diagnostics().commandHighWater == 2);
  CHECK(host.diagnostics().rejectedCommands == 2);
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::CommandQueueFull);

  host.beginFrame(0.0F);
  host.commitCommands();
  CHECK(host.pendingCommandCount() == 1);
  host.beginFrame(0.0F);
  host.commitCommands();
  CHECK(host.pendingCommandCount() == 0);

  REQUIRE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::soundOutputAvailability(true)));
  CHECK_FALSE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::soundOutputAvailability(false)));
  CHECK(host.diagnostics().platformEventHighWater == 1);
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::PlatformEventQueueFull);
  CHECK(host.beginFrame(0.0F).soundOutputAvailable);
}

TEST_CASE("voice intent exposes privacy state levels and rejects unavailable service") {
  cadenza::system::SystemServiceHost host;
  CHECK_FALSE(host.submit(
      cadenza::SystemCommand::setVoiceAnalyzerActive(true)));
  CHECK(host.diagnostics().lastRejection ==
        cadenza::system::SystemRejection::ServiceUnavailable);

  REQUIRE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::microphoneAvailability(true)));
  CHECK(host.beginFrame(0.0F).voice.available);
  REQUIRE(host.submit(
      cadenza::SystemCommand::setVoiceAnalyzerActive(true)));
  const auto& starting = host.commitCommands();
  CHECK(starting.voice.captureState == cadenza::VoiceCaptureState::Starting);
  CHECK(starting.voice.analyzerActive);
  CHECK(starting.voice.microphoneInUse);

  REQUIRE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::voiceCaptureStarted()));
  CHECK(host.beginFrame(0.0F).voice.captureState ==
        cadenza::VoiceCaptureState::Running);
  cadenza::voice::VoiceBlock loud;
  std::fill(loud.samples.begin(), loud.samples.end(), 16384);
  REQUIRE(host.publishVoiceBlock(loud) ==
          cadenza::voice::VoicePublishResult::Accepted);
  REQUIRE(host.publishVoiceBlock(loud) ==
          cadenza::voice::VoicePublishResult::Accepted);
  const auto& analyzed = host.beginFrame(0.0F);
  CHECK(analyzed.voice.rms == doctest::Approx(0.5F));
  CHECK(analyzed.voice.peak == doctest::Approx(0.5F));
  CHECK(analyzed.voice.voiceActive);
}

TEST_CASE("USB voice streaming suppresses cues only when mute setting is enabled") {
  cadenza::system::SystemServiceHost host;
  REQUIRE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::microphoneAvailability(true)));
  REQUIRE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::usbVoiceStreaming(true)));
  host.beginFrame(0.0F);
  REQUIRE(host.submit(cadenza::SystemCommand::playSound(
      cadenza::audio::SoundCue::Confirm)));
  host.commitCommands();
  CHECK(host.diagnostics().suppressedSoundCues == 0);
  CHECK(host.sound().lastAcceptedCue() == cadenza::audio::SoundCue::Confirm);

  REQUIRE(host.submit(cadenza::SystemCommand::setMuteSpeakerDuringUsbMic(true)));
  host.commitCommands();
  REQUIRE(host.submit(cadenza::SystemCommand::playSound(
      cadenza::audio::SoundCue::Navigate)));
  host.commitCommands();
  CHECK(host.diagnostics().suppressedSoundCues == 1);
  CHECK(host.snapshot().muteSpeakerDuringUsbMic);
}

TEST_CASE("USB voice streaming suppresses cues and keeps a visual privacy overlay") {
  cadenza::system::SystemServiceHost host;
  REQUIRE(host.submit(cadenza::SystemCommand::setMuteSpeakerDuringUsbMic(true)));
  host.commitCommands();
  REQUIRE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::microphoneAvailability(true)));
  REQUIRE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::usbVoiceStreaming(true)));
  host.beginFrame(0.0F);
  REQUIRE(host.submit(cadenza::SystemCommand::playSound(
      cadenza::audio::SoundCue::Confirm)));
  host.commitCommands();
  CHECK(host.diagnostics().suppressedSoundCues == 1);
  CHECK(host.sound().pendingCommandCount() == 0);
  CHECK(host.snapshot().voice.usbActive);
  CHECK(host.snapshot().voice.microphoneInUse);

  cadenza::AppRuntime runtime;
  FrameProbeApp app;
  constexpr cadenza::AppId kHome{0x7200};
  REQUIRE(runtime.registerApp(kHome, app, false));
  REQUIRE(runtime.configureHome(kHome));
  REQUIRE(runtime.begin(kHome));
  cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas canvas{framebuffer};
  cadenza::system::FrameCoordinator::runFrame(host, runtime, canvas, 0.0F,
                                               {});
  bool indicatorVisible = false;
  for (int y = 2; y < 14; ++y) {
    for (int x = framebuffer.width() - 31; x < framebuffer.width() - 2; ++x) {
      indicatorVisible = indicatorVisible || framebuffer.pixel(x, y);
    }
  }
  CHECK(indicatorVisible);

  REQUIRE(host.postPlatformEvent(
      cadenza::system::PlatformEvent::usbVoiceStreaming(false)));
  CHECK_FALSE(host.beginFrame(0.0F).voice.microphoneInUse);
  REQUIRE(host.submit(cadenza::SystemCommand::playSound(
      cadenza::audio::SoundCue::Confirm)));
  host.commitCommands();
  CHECK(host.sound().lastAcceptedCue() == cadenza::audio::SoundCue::Confirm);
}
