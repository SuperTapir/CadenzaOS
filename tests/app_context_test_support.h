#pragma once

#include "cadenza/core/app_runtime.h"
#include "cadenza/system/system_service_host.h"

namespace cadenza::test {

inline void updateRuntime(AppRuntime& runtime,
                          system::SystemServiceHost& services, Seconds dt,
                          const InputFrame& input = {}) noexcept {
  const SystemSnapshot snapshot = services.beginFrame(dt);
  runtime.updateWithSystem(dt, input, snapshot, services);
  runtime.bindSystem(services.commitCommands(), services);
}

inline void updateApp(App& app, Seconds dt, const InputFrame& input,
                      AppRuntime& runtime,
                      system::SystemServiceHost& services,
                      AppId caller = {}) noexcept {
  const AppCatalogView catalog = runtime.catalogView();
  const SystemSnapshot snapshot = services.beginFrame(dt);
  services.setCapabilityResolver(&runtime);
  AppCapabilitySet capabilities;
  if (!caller.valid()) caller = runtime.currentId();
  runtime.resolveCapabilities(caller, capabilities);
  AppCommandPort commandPort{caller, capabilities, services};
  const AppUpdateContext context{dt,
                                 input,
                                 catalog,
                                 runtime,
                                 snapshot,
                                 commandPort,
                                 runtime.canvasWidth(),
                                 runtime.canvasHeight()};
  app.update(context);
  runtime.bindSystem(services.commitCommands(), services);
}

inline void renderApp(App& app, MonoCanvas& canvas,
                      const AppRuntime& runtime,
                      const system::SystemServiceHost& services) noexcept {
  const AppCatalogView catalog = runtime.catalogView();
  const AppRenderContext context{catalog, services.snapshot()};
  app.render(canvas, context);
}

}  // namespace cadenza::test
