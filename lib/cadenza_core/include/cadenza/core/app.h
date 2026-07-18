#pragma once

#include "cadenza/core/app_context.h"
#include "cadenza/core/mono_canvas.h"

namespace cadenza {

class App {
 public:
  virtual ~App() = default;
  virtual const char* name() const noexcept = 0;
  virtual void onEnter() noexcept {}
  virtual void onExit() noexcept {}
  virtual void update(const AppUpdateContext& context) noexcept = 0;
  virtual void render(MonoCanvas& canvas,
                      const AppRenderContext& context) noexcept = 0;
  virtual bool renderLauncherCover(MonoCanvas&, Rect) const noexcept {
    return false;
  }
  virtual bool renderLaunchFrame(
      MonoCanvas&, float, const AppRenderContext&) const noexcept {
    return false;
  }
};

}  // namespace cadenza
