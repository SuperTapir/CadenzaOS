#include <cstdlib>
#include <fstream>

#include "cadenza/host/headless_host.h"

namespace {
cadenza::FramebufferProfile profileFrom(const char* value) {
  return value && value[0] == '4' ? cadenza::FramebufferProfile::Sharp
                                  : cadenza::FramebufferProfile::TEmbed;
}

cadenza::AppId appFrom(const char* value) {
  const int app = value ? std::atoi(value) : 0;
  if (app < 0 || app > static_cast<int>(cadenza::AppId::Settings)) {
    return cadenza::AppId::Launcher;
  }
  return static_cast<cadenza::AppId>(app);
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 4) return 2;
  cadenza::host::HeadlessHost host{profileFrom(argv[1])};
  const cadenza::AppId app = appFrom(argv[2]);
  if (app != cadenza::AppId::Launcher) {
    if (!host.runtime().open(app)) return 3;
    for (int frame = 0; frame < 32 && host.runtime().transitioning(); ++frame) {
      host.step();
    }
  }
  const auto& framebuffer = host.framebuffer();
  std::ofstream output{argv[3], std::ios::binary};
  output << "P4\n" << framebuffer.width() << ' ' << framebuffer.height() << '\n';
  output.write(reinterpret_cast<const char*>(framebuffer.data()),
               static_cast<std::streamsize>(framebuffer.sizeBytes()));
  return output ? 0 : 4;
}
