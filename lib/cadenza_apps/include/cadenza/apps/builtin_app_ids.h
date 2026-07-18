#pragma once

#include "cadenza/core/app_capability.h"

namespace cadenza::apps {

inline constexpr AppId kLauncherAppId{0x0100};
inline constexpr AppId kClockAppId{0x0101};
inline constexpr AppId kMotionAppId{0x0102};
inline constexpr AppId kSettingsAppId{0x0103};
inline constexpr AppId kGalleryAppId{0x0104};

constexpr AppCapabilitySet builtinAppCapabilities(AppId id) noexcept {
  if (id == kLauncherAppId) {
    return AppCapabilitySet{AppCapability::SoundPlay} |
           AppCapability::DiagnosticEmit;
  }
  if (id == kSettingsAppId) {
    return AppCapabilitySet{AppCapability::SoundPlay} |
           AppCapability::SettingsWrite |
           AppCapability::NetworkAcquire |
           AppCapability::ProvisioningManage;
  }
  if (id == kClockAppId || id == kMotionAppId || id == kGalleryAppId) {
    return AppCapabilitySet{AppCapability::SoundPlay};
  }
  return {};
}

}  // namespace cadenza::apps
