#include "cadenza/system/system_service_host.h"

#include <algorithm>

namespace cadenza::system {

namespace {
DiagnosticEvent redactedDiagnostic(const DiagnosticEvent& source) noexcept {
  DiagnosticEvent result = source;
  // Text crosses process/CDC/logging boundaries too easily. Portable callers
  // supply only the bounded category/code/value fields; the platform renders a
  // fixed label and never forwards caller-owned text.
  result.message = "structured diagnostic";
  return result;
}
}  // namespace

std::size_t SystemServiceHost::clampCapacity(std::size_t requested,
                                             std::size_t maximum) noexcept {
  return std::max<std::size_t>(1, std::min(requested, maximum));
}

SystemServiceHost::SystemServiceHost(SystemServiceHostConfig config) noexcept
    : config_(config) {
  config_.commandCapacity =
      clampCapacity(config.commandCapacity, kMaxCommandCapacity);
  config_.platformEventCapacity =
      clampCapacity(config.platformEventCapacity, kMaxPlatformEventCapacity);
  config_.commandBudgetPerFrame = std::max<std::size_t>(1,
      std::min(config.commandBudgetPerFrame, config_.commandCapacity));
  config_.platformEventBudgetPerFrame = std::max<std::size_t>(1,
      std::min(config.platformEventBudgetPerFrame,
               config_.platformEventCapacity));
  snapshot_.soundVolume = sound_.volume();
  updateSnapshot_ = snapshot_;
}

bool SystemServiceHost::validCommand(const SystemCommand& command) const noexcept {
  switch (command.type) {
    case SystemCommandType::PlaySound:
      return command.soundCue != audio::SoundCue::Count;
    case SystemCommandType::SetSoundVolume:
      return command.soundVolume != audio::SoundVolume::Count;
    case SystemCommandType::SetMotionProfile:
      return command.motionProfile == MotionProfile::Normal ||
             command.motionProfile == MotionProfile::Reduced;
    case SystemCommandType::SetLauncherOrientation:
      return command.launcherOrientation == LauncherOrientation::Vertical ||
             command.launcherOrientation == LauncherOrientation::Horizontal;
    case SystemCommandType::SetVoiceAnalyzerActive:
    case SystemCommandType::SetNetworkOnlineRequested:
    case SystemCommandType::StartProvisioning:
    case SystemCommandType::CancelProvisioning:
    case SystemCommandType::SetBluetoothAdvertisingRequested:
    case SystemCommandType::SetBluetoothScanningRequested:
      return true;
    case SystemCommandType::EmitDiagnostic:
      return true;
    case SystemCommandType::StartTimer:
    case SystemCommandType::SetTimerRemaining:
      return command.timerDurationMs >= kTimerMinimumDurationMs &&
             command.timerDurationMs <= kTimerMaximumDurationMs;
    case SystemCommandType::PauseTimer:
    case SystemCommandType::ResumeTimer:
    case SystemCommandType::AcknowledgeTimer:
      return true;
  }
  return false;
}

void SystemServiceHost::reject(SystemRejection rejection,
                               bool command) noexcept {
  diagnostics_.lastRejection = rejection;
  if (command) {
    ++diagnostics_.rejectedCommands;
  } else {
    ++diagnostics_.rejectedPlatformEvents;
  }
}

bool SystemServiceHost::submit(
    const SystemOperationEnvelope& operation) noexcept {
  diagnostics_.lastRejectedOwner = operation.owner;
  diagnostics_.lastRejectedCommandType = operation.command.type;
  if (!operation.owner.valid()) {
    reject(SystemRejection::InvalidCaller, true);
    return false;
  }
  if (operation.generation != 0) {
    reject(SystemRejection::StaleGeneration, true);
    return false;
  }
  const SystemCommand& command = operation.command;
  if (!validCommand(command)) {
    reject(SystemRejection::InvalidCommand, true);
    return false;
  }
  if (command.type == SystemCommandType::SetVoiceAnalyzerActive &&
      command.voiceAnalyzerActive &&
      voiceCapture_.state() == VoiceCaptureState::Unavailable) {
    reject(SystemRejection::ServiceUnavailable, true);
    return false;
  }
  if (commandCount_ >= config_.commandCapacity) {
    reject(SystemRejection::CommandQueueFull, true);
    return false;
  }
  commands_[commandTail_] = operation;
  commandTail_ = (commandTail_ + 1) % config_.commandCapacity;
  ++commandCount_;
  ++diagnostics_.submittedCommands;
  diagnostics_.commandHighWater =
      std::max(diagnostics_.commandHighWater, commandCount_);
  return true;
}

void SystemServiceHost::rejectAppOperation(AppId caller,
                                           SystemCommandType type) noexcept {
  diagnostics_.lastRejectedOwner = ResourceOwner::app(caller);
  diagnostics_.lastRejectedCommandType = type;
  reject(caller.valid() ? SystemRejection::CapabilityDenied
                        : SystemRejection::InvalidCaller,
         true);
}

bool SystemServiceHost::postPlatformEvent(const PlatformEvent& event) noexcept {
  if (eventCount_ >= config_.platformEventCapacity) {
    reject(SystemRejection::PlatformEventQueueFull, false);
    return false;
  }
  events_[eventTail_] = event;
  eventTail_ = (eventTail_ + 1) % config_.platformEventCapacity;
  ++eventCount_;
  diagnostics_.platformEventHighWater =
      std::max(diagnostics_.platformEventHighWater, eventCount_);
  return true;
}

void SystemServiceHost::apply(const PlatformEvent& event) noexcept {
  switch (event.type) {
    case PlatformEventType::SoundOutputAvailability:
      snapshot_.soundOutputAvailable = event.available;
      break;
    case PlatformEventType::MicrophoneAvailability:
      voiceCapture_.setAvailable(event.available);
      break;
    case PlatformEventType::VoiceCaptureStarted:
      voiceCapture_.notifyStarted(voice::kVoicePcmFormat);
      break;
    case PlatformEventType::VoiceCaptureError:
      voiceCapture_.notifyError();
      break;
    case PlatformEventType::UsbVoiceStreaming:
      voiceCapture_.setIntent(voice::VoiceConsumer::Usb, event.available);
      break;
    case PlatformEventType::WiFiAvailability:
      connectivity_.setWiFiAvailable(event.available);
      break;
    case PlatformEventType::WiFiRadioReady:
      connectivity_.onWiFiRadioReady();
      break;
    case PlatformEventType::WiFiRadioStopped:
      connectivity_.onWiFiRadioStopped();
      break;
    case PlatformEventType::WiFiAssociated:
      connectivity_.onWiFiAssociated(event.generation);
      break;
    case PlatformEventType::WiFiGotIp:
      connectivity_.onWiFiGotIp(event.generation);
      break;
    case PlatformEventType::WiFiDisconnected:
      connectivity_.onWiFiDisconnected(event.generation, event.wifiFailure);
      break;
    case PlatformEventType::BluetoothLeObserved:
      connectivity_.observeBluetoothLe(event.bluetoothLe);
      break;
    case PlatformEventType::BluetoothLeAvailability:
      connectivity_.setBluetoothLeAvailable(event.available);
      break;
    case PlatformEventType::BluetoothLeRadioReady:
      connectivity_.onBluetoothLeRadioReady(event.generation);
      break;
    case PlatformEventType::BluetoothLeRadioStopped:
      connectivity_.onBluetoothLeRadioStopped(event.generation);
      break;
    case PlatformEventType::ProvisioningProgress:
      connectivity_.onProvisioningProgress(
          event.generation, event.provisioningState,
          event.provisioningFailure);
      break;
    case PlatformEventType::ProvisioningStopped:
      connectivity_.onProvisioningStopped(event.generation);
      break;
  }
}

bool SystemServiceHost::apply(
    const SystemOperationEnvelope& operation) noexcept {
  const SystemCommand& command = operation.command;
  switch (command.type) {
    case SystemCommandType::PlaySound:
      if (voiceCapture_.intentActive(voice::VoiceConsumer::Usb)) {
        ++diagnostics_.suppressedSoundCues;
      } else {
        sound_.play(command.soundCue);
      }
      return true;
    case SystemCommandType::SetSoundVolume:
      sound_.setVolume(command.soundVolume);
      snapshot_.soundVolume = sound_.volume();
      return true;
    case SystemCommandType::SetMotionProfile:
      snapshot_.motionProfile = command.motionProfile;
      return true;
    case SystemCommandType::SetLauncherOrientation:
      snapshot_.launcherOrientation = command.launcherOrientation;
      return true;
    case SystemCommandType::SetVoiceAnalyzerActive:
      if (command.voiceAnalyzerActive) {
        leases_.acquire(
            operation.owner, SystemResource::VoiceAnalyzer,
            operation.owner.kind == ResourceOwnerKind::App
                ? LeaseLifetime::Foreground
                : LeaseLifetime::Session,
            operation.generation);
      } else {
        leases_.release(operation.owner, SystemResource::VoiceAnalyzer);
      }
      voiceCapture_.setIntent(
          voice::VoiceConsumer::Analyzer,
          leases_.desired(SystemResource::VoiceAnalyzer));
      return true;
    case SystemCommandType::SetNetworkOnlineRequested:
      if (command.networkOnlineRequested) {
        leases_.acquire(
            operation.owner, SystemResource::Network,
            operation.owner.kind == ResourceOwnerKind::App
                ? LeaseLifetime::Foreground
                : LeaseLifetime::Session,
            operation.generation);
      } else {
        leases_.release(operation.owner, SystemResource::Network);
      }
      connectivity_.requestNetworkOnline(
          leases_.desired(SystemResource::Network));
      return true;
    case SystemCommandType::StartProvisioning: {
      const ProvisioningRequestResult result = connectivity_.startProvisioning(
          operation.owner, command.resetCredentialsConfirmed);
      if (result == ProvisioningRequestResult::Started) {
        leases_.acquire(operation.owner, SystemResource::ProvisioningSession,
                        LeaseLifetime::Session,
                        connectivity_.snapshot().provisioning.generation);
        return true;
      }
      reject(result == ProvisioningRequestResult::Busy
                 ? SystemRejection::ServiceBusy
                 : SystemRejection::ServiceUnavailable,
             true);
      return false;
    }
    case SystemCommandType::CancelProvisioning: {
      const ProvisioningRequestResult result =
          connectivity_.cancelProvisioning(operation.owner);
      if (result == ProvisioningRequestResult::Cancelled) return true;
      reject(result == ProvisioningRequestResult::Unauthorized
                 ? SystemRejection::NotOwner
                 : SystemRejection::ServiceUnavailable,
             true);
      return false;
    }
    case SystemCommandType::SetBluetoothAdvertisingRequested:
      if (command.bluetoothRoleRequested) {
        leases_.acquire(
            operation.owner, SystemResource::BluetoothAdvertising,
            operation.owner.kind == ResourceOwnerKind::App
                ? LeaseLifetime::Foreground
                : LeaseLifetime::Session,
            operation.generation);
      } else {
        leases_.release(operation.owner,
                        SystemResource::BluetoothAdvertising);
      }
      connectivity_.requestBluetoothAdvertising(
          leases_.desired(SystemResource::BluetoothAdvertising));
      return true;
    case SystemCommandType::SetBluetoothScanningRequested:
      if (command.bluetoothRoleRequested) {
        leases_.acquire(
            operation.owner, SystemResource::BluetoothScanning,
            operation.owner.kind == ResourceOwnerKind::App
                ? LeaseLifetime::Foreground
                : LeaseLifetime::Session,
            operation.generation);
      } else {
        leases_.release(operation.owner, SystemResource::BluetoothScanning);
      }
      connectivity_.requestBluetoothScanning(
          leases_.desired(SystemResource::BluetoothScanning));
      return true;
    case SystemCommandType::EmitDiagnostic:
      if (diagnosticSink_) {
        diagnosticSink_->emit(redactedDiagnostic(command.diagnostic));
      }
      return true;
    case SystemCommandType::StartTimer:
    case SystemCommandType::PauseTimer:
    case SystemCommandType::ResumeTimer:
    case SystemCommandType::SetTimerRemaining: {
      if (operation.owner.kind != ResourceOwnerKind::App) {
        reject(SystemRejection::InvalidCaller, true);
        return false;
      }
      TimerRequestResult result = TimerRequestResult::InvalidState;
      if (command.type == SystemCommandType::StartTimer) {
        result = timer_.start(operation.owner.appId, command.timerDurationMs);
      } else if (command.type == SystemCommandType::PauseTimer) {
        result = timer_.pause(operation.owner.appId);
      } else if (command.type == SystemCommandType::ResumeTimer) {
        result = timer_.resume(operation.owner.appId);
      } else {
        result = timer_.setRemaining(operation.owner.appId,
                                     command.timerDurationMs);
      }
      snapshot_.timer = timer_.snapshot();
      if (result == TimerRequestResult::Accepted) return true;
      reject(result == TimerRequestResult::NotOwner
                 ? SystemRejection::NotOwner
                 : result == TimerRequestResult::InvalidDuration
                       ? SystemRejection::InvalidDuration
                       : SystemRejection::InvalidState,
             true);
      return false;
    }
    case SystemCommandType::AcknowledgeTimer: {
      if (operation.owner.kind != ResourceOwnerKind::System) {
        reject(SystemRejection::InvalidCaller, true);
        return false;
      }
      const TimerRequestResult result = timer_.acknowledge();
      snapshot_.timer = timer_.snapshot();
      if (result == TimerRequestResult::Accepted) {
        nextTimerAlertMs_ = 0;
        sound_.stopAll();
        return true;
      }
      reject(SystemRejection::InvalidState, true);
      return false;
    }
  }
  return false;
}

const SystemSnapshot& SystemServiceHost::beginFrame(Seconds dt) noexcept {
  const double elapsedMs = static_cast<double>(std::max(0.0F, dt)) * 1000.0 +
                           fallbackSubmillis_;
  const auto wholeMs = static_cast<MonotonicMillis>(elapsedMs);
  fallbackSubmillis_ = elapsedMs - static_cast<double>(wholeMs);
  fallbackNowMs_ += wholeMs;
  return beginFrameAt(fallbackNowMs_, dt);
}

const SystemSnapshot& SystemServiceHost::beginFrameAt(
    MonotonicMillis nowMs, Seconds presentationDt) noexcept {
  if (nowMs >= fallbackNowMs_) {
    fallbackNowMs_ = nowMs;
    fallbackSubmillis_ = 0.0;
  }
  const bool expiredNow = timer_.advanceTo(nowMs);
  snapshot_.timer = timer_.snapshot();
  if (expiredNow) {
    sound_.play(audio::SoundCue::TimerComplete);
    nextTimerAlertMs_ = nowMs + kTimerAlertRepeatMs;
  } else if (snapshot_.timer.state == TimerState::Expired &&
             nextTimerAlertMs_ != 0 && nowMs >= nextTimerAlertMs_) {
    sound_.play(audio::SoundCue::TimerComplete);
    nextTimerAlertMs_ = nowMs + kTimerAlertRepeatMs;
  }
  sound_.advance(std::max(0.0F, presentationDt));
  const std::size_t eventBudget =
      std::min(config_.platformEventBudgetPerFrame, eventCount_);
  for (std::size_t index = 0; index < eventBudget; ++index) {
    apply(events_[eventHead_]);
    eventHead_ = (eventHead_ + 1) % config_.platformEventCapacity;
    --eventCount_;
    ++diagnostics_.ingestedPlatformEvents;
  }
  connectivity_.advance(static_cast<std::uint32_t>(
      std::max(0.0F, presentationDt) * 1000.0F));
  ResourceOwner releasedProvisioningOwner;
  while (connectivity_.takeReleasedProvisioningOwner(
      releasedProvisioningOwner)) {
    leases_.release(releasedProvisioningOwner,
                    SystemResource::ProvisioningSession);
  }
  snapshot_.connectivity = connectivity_.snapshot();
  processVoiceAnalyzer();
  refreshVoiceSnapshot();
  updateSnapshot_ = snapshot_;
  return updateSnapshot_;
}

const SystemSnapshot& SystemServiceHost::commitCommands() noexcept {
  const std::size_t commandBudget =
      std::min(config_.commandBudgetPerFrame, commandCount_);
  for (std::size_t index = 0; index < commandBudget; ++index) {
    const SystemOperationEnvelope operation = commands_[commandHead_];
    commandHead_ = (commandHead_ + 1) % config_.commandCapacity;
    --commandCount_;
    if (operation.owner.kind == ResourceOwnerKind::App) {
      diagnostics_.lastRejectedOwner = operation.owner;
      diagnostics_.lastRejectedCommandType = operation.command.type;
      AppCapabilitySet capabilities;
      if (!capabilityResolver_ ||
          !capabilityResolver_->resolveCapabilities(operation.owner.appId,
                                                    capabilities)) {
        reject(SystemRejection::InvalidCaller, true);
        continue;
      }
      if (!capabilities.contains(requiredCapability(operation.command.type))) {
        reject(SystemRejection::CapabilityDenied, true);
        continue;
      }
    }
    if (apply(operation)) ++diagnostics_.committedCommands;
  }
  snapshot_.connectivity = connectivity_.snapshot();
  refreshVoiceSnapshot();
  return snapshot_;
}

std::size_t SystemServiceHost::releaseForeground(ResourceOwner owner) noexcept {
  const std::size_t released = leases_.releaseForeground(owner);
  voiceCapture_.setIntent(voice::VoiceConsumer::Analyzer,
                          leases_.desired(SystemResource::VoiceAnalyzer));
  connectivity_.requestNetworkOnline(
      leases_.desired(SystemResource::Network));
  connectivity_.requestBluetoothAdvertising(
      leases_.desired(SystemResource::BluetoothAdvertising));
  connectivity_.requestBluetoothScanning(
      leases_.desired(SystemResource::BluetoothScanning));
  snapshot_.connectivity = connectivity_.snapshot();
  refreshVoiceSnapshot();
  return released;
}

void SystemServiceHost::processVoiceAnalyzer() noexcept {
  voice::VoiceBlock block;
  while (voiceCapture_.tryPop(voice::VoiceConsumer::Analyzer, block)) {
    voiceAnalyzer_.process(block);
  }
}

void SystemServiceHost::refreshVoiceSnapshot() noexcept {
  const auto analyzerDiagnostics =
      voiceCapture_.consumerDiagnostics(voice::VoiceConsumer::Analyzer);
  const auto usbDiagnostics =
      voiceCapture_.consumerDiagnostics(voice::VoiceConsumer::Usb);
  const auto& analyzer = voiceAnalyzer_.snapshot();
  snapshot_.voice.captureState = voiceCapture_.state();
  snapshot_.voice.available =
      voiceCapture_.state() != VoiceCaptureState::Unavailable;
  snapshot_.voice.analyzerActive =
      voiceCapture_.intentActive(voice::VoiceConsumer::Analyzer);
  snapshot_.voice.usbActive =
      voiceCapture_.intentActive(voice::VoiceConsumer::Usb);
  snapshot_.voice.microphoneInUse = voiceCapture_.microphoneInUse();
  snapshot_.voice.voiceActive = analyzer.voiceActive;
  snapshot_.voice.rms = analyzer.rms;
  snapshot_.voice.peak = analyzer.peak;
  snapshot_.voice.analyzerOverruns = analyzerDiagnostics.overruns;
  snapshot_.voice.usbOverruns = usbDiagnostics.overruns;
}

}  // namespace cadenza::system
