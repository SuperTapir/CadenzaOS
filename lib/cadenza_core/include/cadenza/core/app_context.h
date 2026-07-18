#pragma once

#include <cstddef>
#include <cstdint>

#include "cadenza/audio/interaction_sound.h"
#include "cadenza/core/app_capability.h"
#include "cadenza/core/app_catalog.h"
#include "cadenza/core/connectivity_types.h"
#include "cadenza/core/core_types.h"
#include "cadenza/core/diagnostics.h"
#include "cadenza/core/input.h"
#include "cadenza/core/voice_types.h"

namespace cadenza {

class MonoCanvas;
struct Rect;

class AppCatalogView {
 public:
  explicit constexpr AppCatalogView(const AppCatalog& catalog) noexcept
      : catalog_(&catalog) {}

  std::uint8_t launcherAppCount() const noexcept {
    std::uint8_t count = 0;
    for (std::size_t index = 0; index < catalog_->size(); ++index) {
      if (catalog_->at(index)->visibleInLauncher) ++count;
    }
    return count;
  }

  AppId launcherAppAt(std::uint8_t position) const noexcept {
    for (std::size_t index = 0; index < catalog_->size(); ++index) {
      const AppCatalogEntry* entry = catalog_->at(index);
      if (entry->visibleInLauncher && position-- == 0) return entry->id;
    }
    return {};
  }

  const char* appName(AppId id) const noexcept {
    if (!id.valid()) return "INVALID";
    const AppCatalogEntry* entry = catalog_->find(id);
    return entry ? entry->name : "MISSING";
  }

  bool renderLauncherCover(AppId id, MonoCanvas& canvas,
                           Rect bounds) const noexcept;
  bool renderLaunchFrame(AppId id, MonoCanvas& canvas,
                         float progress) const noexcept;

 private:
  const AppCatalog* catalog_;
};

class AppNavigator {
 public:
  virtual ~AppNavigator() = default;
  virtual bool open(AppId id) noexcept = 0;
};

struct SystemSnapshot {
  audio::SoundVolume soundVolume = audio::SoundVolume::High;
  MotionProfile motionProfile = MotionProfile::Normal;
  LauncherOrientation launcherOrientation = LauncherOrientation::Vertical;
  bool soundOutputAvailable = false;
  ConnectivitySnapshot connectivity{};
  VoiceSnapshot voice{};
};

enum class SystemCommandType : std::uint8_t {
  PlaySound,
  SetSoundVolume,
  SetMotionProfile,
  SetLauncherOrientation,
  SetVoiceAnalyzerActive,
  SetNetworkOnlineRequested,
  StartProvisioning,
  CancelProvisioning,
  SetBluetoothAdvertisingRequested,
  SetBluetoothScanningRequested,
  EmitDiagnostic,
};

constexpr AppCapability requiredCapability(
    SystemCommandType type) noexcept {
  switch (type) {
    case SystemCommandType::PlaySound:
      return AppCapability::SoundPlay;
    case SystemCommandType::SetSoundVolume:
    case SystemCommandType::SetMotionProfile:
    case SystemCommandType::SetLauncherOrientation:
      return AppCapability::SettingsWrite;
    case SystemCommandType::SetVoiceAnalyzerActive:
      return AppCapability::VoiceAnalyzer;
    case SystemCommandType::SetNetworkOnlineRequested:
      return AppCapability::NetworkAcquire;
    case SystemCommandType::StartProvisioning:
    case SystemCommandType::CancelProvisioning:
      return AppCapability::ProvisioningManage;
    case SystemCommandType::SetBluetoothAdvertisingRequested:
    case SystemCommandType::SetBluetoothScanningRequested:
      return AppCapability::BluetoothControl;
    case SystemCommandType::EmitDiagnostic:
      return AppCapability::DiagnosticEmit;
  }
  return AppCapability::Count;
}

struct SystemCommand {
  SystemCommandType type = SystemCommandType::PlaySound;
  audio::SoundCue soundCue = audio::SoundCue::Navigate;
  audio::SoundVolume soundVolume = audio::SoundVolume::High;
  MotionProfile motionProfile = MotionProfile::Normal;
  LauncherOrientation launcherOrientation = LauncherOrientation::Vertical;
  bool voiceAnalyzerActive = false;
  bool networkOnlineRequested = false;
  bool resetCredentialsConfirmed = false;
  bool bluetoothRoleRequested = false;
  DiagnosticEvent diagnostic{};

  static SystemCommand playSound(audio::SoundCue cue) noexcept {
    SystemCommand command;
    command.type = SystemCommandType::PlaySound;
    command.soundCue = cue;
    return command;
  }

  static SystemCommand setSoundVolume(audio::SoundVolume volume) noexcept {
    SystemCommand command;
    command.type = SystemCommandType::SetSoundVolume;
    command.soundVolume = volume;
    return command;
  }

  static SystemCommand setMotionProfile(MotionProfile profile) noexcept {
    SystemCommand command;
    command.type = SystemCommandType::SetMotionProfile;
    command.motionProfile = profile;
    return command;
  }

  static SystemCommand setLauncherOrientation(
      LauncherOrientation orientation) noexcept {
    SystemCommand command;
    command.type = SystemCommandType::SetLauncherOrientation;
    command.launcherOrientation = orientation;
    return command;
  }

  static SystemCommand setVoiceAnalyzerActive(bool active) noexcept {
    SystemCommand command;
    command.type = SystemCommandType::SetVoiceAnalyzerActive;
    command.voiceAnalyzerActive = active;
    return command;
  }

  static SystemCommand setNetworkOnlineRequested(bool requested) noexcept {
    SystemCommand command;
    command.type = SystemCommandType::SetNetworkOnlineRequested;
    command.networkOnlineRequested = requested;
    return command;
  }

  static SystemCommand startProvisioning(
      bool resetCredentialsConfirmed = false) noexcept {
    SystemCommand command;
    command.type = SystemCommandType::StartProvisioning;
    command.resetCredentialsConfirmed = resetCredentialsConfirmed;
    return command;
  }

  static SystemCommand cancelProvisioning() noexcept {
    SystemCommand command;
    command.type = SystemCommandType::CancelProvisioning;
    return command;
  }

  static SystemCommand setBluetoothAdvertisingRequested(
      bool requested) noexcept {
    SystemCommand command;
    command.type = SystemCommandType::SetBluetoothAdvertisingRequested;
    command.bluetoothRoleRequested = requested;
    return command;
  }

  static SystemCommand setBluetoothScanningRequested(bool requested) noexcept {
    SystemCommand command;
    command.type = SystemCommandType::SetBluetoothScanningRequested;
    command.bluetoothRoleRequested = requested;
    return command;
  }

  static SystemCommand emitDiagnostic(const DiagnosticEvent& event) noexcept {
    SystemCommand command;
    command.type = SystemCommandType::EmitDiagnostic;
    command.diagnostic = event;
    return command;
  }
};

enum class ResourceOwnerKind : std::uint8_t {
  Invalid,
  System,
  Usb,
  App,
};

struct ResourceOwner {
  ResourceOwnerKind kind = ResourceOwnerKind::Invalid;
  AppId appId{};

  static constexpr ResourceOwner system() noexcept {
    return {ResourceOwnerKind::System, {}};
  }
  static constexpr ResourceOwner usb() noexcept {
    return {ResourceOwnerKind::Usb, {}};
  }
  static constexpr ResourceOwner app(AppId id) noexcept {
    return {ResourceOwnerKind::App, id};
  }
  constexpr bool valid() const noexcept {
    return kind == ResourceOwnerKind::System ||
           kind == ResourceOwnerKind::Usb ||
           (kind == ResourceOwnerKind::App && appId.valid());
  }
  friend constexpr bool operator==(ResourceOwner lhs,
                                   ResourceOwner rhs) noexcept {
    return lhs.kind == rhs.kind && lhs.appId == rhs.appId;
  }
};

struct SystemOperationEnvelope {
  ResourceOwner owner{};
  SystemCommand command{};
  std::uint32_t generation = 0;
};

class SystemCommandSink {
 public:
  virtual ~SystemCommandSink() = default;
  virtual void bindCapabilityResolver(
      const AppCapabilityResolver& resolver) noexcept = 0;
  virtual bool submit(const SystemOperationEnvelope& operation) noexcept = 0;
  virtual void rejectAppOperation(AppId caller,
                                  SystemCommandType type) noexcept = 0;
};

class AppCommandPort {
 public:
  AppCommandPort(AppId caller, AppCapabilitySet capabilities,
                 SystemCommandSink& sink) noexcept
      : caller_(caller), capabilities_(capabilities), sink_(&sink) {}

  bool submit(const SystemCommand& command) const noexcept {
    const AppCapability capability = requiredCapability(command.type);
    if (!caller_.valid() || !capabilities_.contains(capability)) {
      sink_->rejectAppOperation(caller_, command.type);
      return false;
    }
    return sink_->submit({ResourceOwner::app(caller_), command, 0});
  }

 private:
  AppId caller_{};
  AppCapabilitySet capabilities_{};
  SystemCommandSink* sink_ = nullptr;
};

struct AppUpdateContext {
  Seconds dt;
  const InputFrame& input;
  const AppCatalogView& catalog;
  AppNavigator& navigator;
  const SystemSnapshot& system;
  AppCommandPort& commands;
  std::int16_t displayWidth;
  std::int16_t displayHeight;
};

struct AppRenderContext {
  const AppCatalogView& catalog;
  const SystemSnapshot& system;
};

}  // namespace cadenza
