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
  if (app < 0 || app > static_cast<int>(cadenza::AppId::Gallery)) {
    return cadenza::AppId::Launcher;
  }
  return static_cast<cadenza::AppId>(app);
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 4 || argc > 6) return 2;
  cadenza::host::HeadlessHost host{profileFrom(argv[1])};
  const cadenza::AppId app = appFrom(argv[2]);
  if (app != cadenza::AppId::Launcher) {
    if (!host.runtime().open(app)) return 3;
    for (int frame = 0; frame < 32 && host.runtime().transitioning(); ++frame) {
      host.step();
    }
  }
  if (argc == 5 && app == cadenza::AppId::Gallery) {
    cadenza::InputFrame input;
    input.turn = static_cast<std::int16_t>(std::atoi(argv[4]));
    host.step(input);
  }
  if (argc == 6 && app == cadenza::AppId::Gallery) {
    cadenza::InputFrame navigate;
    navigate.turn = static_cast<std::int16_t>(std::atoi(argv[4]));
    host.step(navigate);
    cadenza::InputFrame scrubMode;
    scrubMode.clicked = true;
    host.step(scrubMode);
    cadenza::InputFrame scrub;
    scrub.turn = static_cast<std::int16_t>(std::atoi(argv[5]));
    host.step(scrub);
  }
  const auto& framebuffer = host.framebuffer();
  std::ofstream output{argv[3], std::ios::binary};
  output << "P4\n" << framebuffer.width() << ' ' << framebuffer.height() << '\n';
  output.write(reinterpret_cast<const char*>(framebuffer.data()),
               static_cast<std::streamsize>(framebuffer.sizeBytes()));
  return output ? 0 : 4;
}
