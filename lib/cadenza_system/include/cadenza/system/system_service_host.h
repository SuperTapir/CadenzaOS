#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "cadenza/core/app_context.h"
#include "cadenza/system/system_resource_leases.h"
#include "cadenza/system/connectivity_service.h"
#include "cadenza/voice/voice_input.h"

namespace cadenza::system {

enum class PlatformEventType : std::uint8_t {
  SoundOutputAvailability,
  MicrophoneAvailability,
  VoiceCaptureStarted,
  VoiceCaptureError,
  UsbVoiceStreaming,
  WiFiAvailability,
  WiFiRadioReady,
  WiFiRadioStopped,
  WiFiAssociated,
  WiFiGotIp,
  WiFiDisconnected,
  BluetoothLeObserved,
  BluetoothLeAvailability,
  BluetoothLeRadioReady,
  BluetoothLeRadioStopped,
  ProvisioningProgress,
  ProvisioningStopped,
};

struct PlatformEvent {
  PlatformEventType type = PlatformEventType::SoundOutputAvailability;
  bool available = false;
  std::uint32_t generation = 0;
  WiFiFailure wifiFailure = WiFiFailure::None;
  BluetoothLeSnapshot bluetoothLe{};
  ProvisioningState provisioningState = ProvisioningState::Inactive;
  ProvisioningFailure provisioningFailure = ProvisioningFailure::None;

  static constexpr PlatformEvent soundOutputAvailability(
      bool value) noexcept {
    return {PlatformEventType::SoundOutputAvailability, value};
  }
  static constexpr PlatformEvent microphoneAvailability(bool value) noexcept {
    return {PlatformEventType::MicrophoneAvailability, value};
  }
  static constexpr PlatformEvent voiceCaptureStarted() noexcept {
    return {PlatformEventType::VoiceCaptureStarted, true};
  }
  static constexpr PlatformEvent voiceCaptureError() noexcept {
    return {PlatformEventType::VoiceCaptureError, true};
  }
  static constexpr PlatformEvent usbVoiceStreaming(bool value) noexcept {
    return {PlatformEventType::UsbVoiceStreaming, value};
  }
  static constexpr PlatformEvent wifiAvailability(bool available) noexcept {
    return {PlatformEventType::WiFiAvailability, available};
  }
  static constexpr PlatformEvent wifiRadioReady() noexcept {
    return {PlatformEventType::WiFiRadioReady, true};
  }
  static constexpr PlatformEvent wifiRadioStopped() noexcept {
    return {PlatformEventType::WiFiRadioStopped, true};
  }
  static constexpr PlatformEvent wifiAssociated(
      std::uint32_t generation) noexcept {
    PlatformEvent event;
    event.type = PlatformEventType::WiFiAssociated;
    event.generation = generation;
    return event;
  }
  static constexpr PlatformEvent wifiGotIp(std::uint32_t generation) noexcept {
    PlatformEvent event;
    event.type = PlatformEventType::WiFiGotIp;
    event.generation = generation;
    return event;
  }
  static constexpr PlatformEvent wifiDisconnected(
      std::uint32_t generation, WiFiFailure failure) noexcept {
    PlatformEvent event;
    event.type = PlatformEventType::WiFiDisconnected;
    event.generation = generation;
    event.wifiFailure = failure;
    return event;
  }
  static constexpr PlatformEvent bluetoothObserved(
      const BluetoothLeSnapshot& snapshot) noexcept {
    PlatformEvent event;
    event.type = PlatformEventType::BluetoothLeObserved;
    event.bluetoothLe = snapshot;
    return event;
  }
  static constexpr PlatformEvent bluetoothAvailability(bool available) noexcept {
    return {PlatformEventType::BluetoothLeAvailability, available};
  }
  static constexpr PlatformEvent bluetoothRadioReady(
      std::uint32_t generation) noexcept {
    PlatformEvent event;
    event.type = PlatformEventType::BluetoothLeRadioReady;
    event.generation = generation;
    return event;
  }
  static constexpr PlatformEvent bluetoothRadioStopped(
      std::uint32_t generation) noexcept {
    PlatformEvent event;
    event.type = PlatformEventType::BluetoothLeRadioStopped;
    event.generation = generation;
    return event;
  }
  static constexpr PlatformEvent provisioningProgress(
      std::uint32_t generation, ProvisioningState state,
      ProvisioningFailure failure = ProvisioningFailure::None) noexcept {
    PlatformEvent event;
    event.type = PlatformEventType::ProvisioningProgress;
    event.generation = generation;
    event.provisioningState = state;
    event.provisioningFailure = failure;
    return event;
  }
  static constexpr PlatformEvent provisioningStopped(
      std::uint32_t generation) noexcept {
    PlatformEvent event;
    event.type = PlatformEventType::ProvisioningStopped;
    event.generation = generation;
    return event;
  }
};

enum class SystemRejection : std::uint8_t {
  None,
  CommandQueueFull,
  PlatformEventQueueFull,
  InvalidCommand,
  ServiceUnavailable,
  InvalidCaller,
  CapabilityDenied,
  StaleGeneration,
  ServiceBusy,
  NotOwner,
};

struct SystemServiceHostConfig {
  std::size_t commandCapacity = 16;
  std::size_t platformEventCapacity = 16;
  std::size_t commandBudgetPerFrame = 16;
  std::size_t platformEventBudgetPerFrame = 16;
};

struct SystemServiceDiagnostics {
  std::uint32_t submittedCommands = 0;
  std::uint32_t committedCommands = 0;
  std::uint32_t rejectedCommands = 0;
  std::uint32_t ingestedPlatformEvents = 0;
  std::uint32_t rejectedPlatformEvents = 0;
  std::size_t commandHighWater = 0;
  std::size_t platformEventHighWater = 0;
  SystemRejection lastRejection = SystemRejection::None;
  std::uint32_t suppressedSoundCues = 0;
  ResourceOwner lastRejectedOwner{};
  SystemCommandType lastRejectedCommandType = SystemCommandType::PlaySound;
};

class SystemServiceHost final : public SystemCommandSink {
 public:
  static constexpr std::size_t kMaxCommandCapacity = 32;
  static constexpr std::size_t kMaxPlatformEventCapacity = 32;

  explicit SystemServiceHost(
      SystemServiceHostConfig config = {}) noexcept;

  bool submit(const SystemOperationEnvelope& operation) noexcept override;
  void bindCapabilityResolver(
      const AppCapabilityResolver& resolver) noexcept override {
    capabilityResolver_ = &resolver;
  }
  bool submit(const SystemCommand& command) noexcept {
    return submit({ResourceOwner::system(), command, 0});
  }
  void rejectAppOperation(AppId caller,
                          SystemCommandType type) noexcept override;
  void setCapabilityResolver(const AppCapabilityResolver* resolver) noexcept {
    capabilityResolver_ = resolver;
  }
  bool postPlatformEvent(const PlatformEvent& event) noexcept;
  voice::VoicePublishResult publishVoiceBlock(
      const voice::VoiceBlock& block,
      voice::VoicePcmFormat format = voice::kVoicePcmFormat) noexcept {
    return voiceCapture_.publish(block, format);
  }
  const SystemSnapshot& beginFrame(Seconds dt) noexcept;
  const SystemSnapshot& commitCommands() noexcept;

  const SystemSnapshot& snapshot() const noexcept { return snapshot_; }
  const SystemServiceDiagnostics& diagnostics() const noexcept {
    return diagnostics_;
  }
  const SystemResourceLeaseTable& leases() const noexcept { return leases_; }
  ConnectivityService& connectivityService() noexcept { return connectivity_; }
  const ConnectivityService& connectivityService() const noexcept {
    return connectivity_;
  }
  std::size_t releaseForeground(ResourceOwner owner) noexcept;
  std::size_t pendingCommandCount() const noexcept { return commandCount_; }
  std::size_t pendingPlatformEventCount() const noexcept {
    return eventCount_;
  }

  audio::InteractionSoundService& sound() noexcept { return sound_; }
  const audio::InteractionSoundService& sound() const noexcept { return sound_; }
  void renderAudio(std::int16_t* samples, std::size_t count) noexcept {
    sound_.render(samples, count);
  }
  void setDiagnosticSink(DiagnosticSink* sink) noexcept { diagnosticSink_ = sink; }
  voice::VoiceCaptureCoordinator& voiceCapture() noexcept {
    return voiceCapture_;
  }
  const voice::VoiceCaptureCoordinator& voiceCapture() const noexcept {
    return voiceCapture_;
  }

 private:
  static std::size_t clampCapacity(std::size_t requested,
                                   std::size_t maximum) noexcept;
  bool validCommand(const SystemCommand& command) const noexcept;
  void reject(SystemRejection rejection, bool command) noexcept;
  void apply(const PlatformEvent& event) noexcept;
  bool apply(const SystemOperationEnvelope& operation) noexcept;
  void refreshVoiceSnapshot() noexcept;
  void processVoiceAnalyzer() noexcept;

  SystemServiceHostConfig config_;
  std::array<SystemOperationEnvelope, kMaxCommandCapacity> commands_{};
  std::array<PlatformEvent, kMaxPlatformEventCapacity> events_{};
  std::size_t commandHead_ = 0;
  std::size_t commandTail_ = 0;
  std::size_t commandCount_ = 0;
  std::size_t eventHead_ = 0;
  std::size_t eventTail_ = 0;
  std::size_t eventCount_ = 0;
  SystemSnapshot snapshot_{};
  SystemSnapshot updateSnapshot_{};
  SystemServiceDiagnostics diagnostics_{};
  audio::InteractionSoundService sound_{};
  voice::VoiceCaptureCoordinator voiceCapture_{};
  voice::VoiceAnalyzer voiceAnalyzer_{};
  SystemResourceLeaseTable leases_{};
  ConnectivityService connectivity_{};
  DiagnosticSink* diagnosticSink_ = nullptr;
  const AppCapabilityResolver* capabilityResolver_ = nullptr;
};

}  // namespace cadenza::system
