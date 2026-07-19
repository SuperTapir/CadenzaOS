#include <iostream>

#include "cadenza/core/app_runtime.h"
#include "cadenza/core/mono_framebuffer.h"
#include "cadenza/presentation/system_surface.h"
#include "cadenza/system/timer_service.h"

static_assert(sizeof(cadenza::AppRuntime) <
                  sizeof(cadenza::MonoFramebuffer) * 3,
              "System Overlay must not add a third maximum framebuffer");

int main() {
  std::cout << "MonoFramebuffer=" << sizeof(cadenza::MonoFramebuffer) << '\n'
            << "SystemSurfaceCoordinator="
            << sizeof(cadenza::presentation::SystemSurfaceCoordinator) << '\n'
            << "SystemSnapshot=" << sizeof(cadenza::SystemSnapshot) << '\n'
            << "TimerService=" << sizeof(cadenza::system::TimerService) << '\n'
            << "AppRuntime=" << sizeof(cadenza::AppRuntime) << '\n';
  return 0;
}
