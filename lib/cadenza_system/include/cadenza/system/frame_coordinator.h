#pragma once

#include "cadenza/core/app_runtime.h"
#include "cadenza/system/system_service_host.h"

namespace cadenza::system {

class FrameCoordinator {
 public:
  static void runFrame(SystemServiceHost& services, AppRuntime& runtime,
                       MonoCanvas& canvas, Seconds dt,
                       const InputFrame& input) noexcept {
    services.setCapabilityResolver(&runtime);
    const SystemSnapshot& updateSnapshot = services.beginFrame(dt);
    runtime.bindSystem(updateSnapshot, services);
    const AppId previousApp = runtime.currentId();
    runtime.updateWithSystem(dt, input, updateSnapshot, services);
    if (runtime.currentId() != previousApp && previousApp.valid()) {
      services.releaseForeground(ResourceOwner::app(previousApp));
    }
    const SystemSnapshot& renderSnapshot = services.commitCommands();
    runtime.renderWithSystem(canvas, renderSnapshot);
    if (renderSnapshot.voice.microphoneInUse) {
      canvas.fillRect(canvas.width() - 31, 2, 29, 12, true);
      canvas.text("MIC", canvas.width() - 16, 8, 1, false,
                  TextAlign::MiddleCenter);
    }
  }
};

}  // namespace cadenza::system
