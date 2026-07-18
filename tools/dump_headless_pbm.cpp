#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include "cadenza/host/headless_host.h"

namespace {
cadenza::FramebufferProfile profileFrom(const char* value) {
  return value && value[0] == '4' ? cadenza::FramebufferProfile::Sharp
                                  : cadenza::FramebufferProfile::TEmbed;
}

cadenza::AppId appFrom(const char* value) {
  const int app = value ? std::atoi(value) : 0;
  switch (app) {
    case 0: return cadenza::apps::kLauncherAppId;
    case 1: return cadenza::apps::kClockAppId;
    case 2: return cadenza::apps::kMotionAppId;
    case 3: return cadenza::apps::kSettingsAppId;
    case 4: return cadenza::apps::kGalleryAppId;
    default: return cadenza::apps::kLauncherAppId;
  }
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 4 || argc > 7) return 2;
  cadenza::host::HeadlessHost host{profileFrom(argv[1])};
  const cadenza::AppId app = appFrom(argv[2]);
  if (app != cadenza::apps::kLauncherAppId) {
    if (!host.runtime().open(app)) return 3;
    for (int frame = 0; frame < 32 && host.runtime().transitioning(); ++frame) {
      host.step();
    }
  }
  if (argc >= 5 && app == cadenza::apps::kGalleryAppId) {
    cadenza::InputFrame input;
    input.turn = static_cast<std::int16_t>(std::atoi(argv[4]));
    host.step(input);
  }
  if (argc == 6 && app == cadenza::apps::kGalleryAppId) {
    cadenza::InputFrame scrubMode;
    scrubMode.clicked = true;
    host.step(scrubMode);
    cadenza::InputFrame scrub;
    scrub.turn = static_cast<std::int16_t>(std::atoi(argv[5]));
    host.step(scrub);
  }
  if (argc == 7 && app == cadenza::apps::kGalleryAppId) {
    if (std::strcmp(argv[5], "auto") != 0) return 2;
    const int frames = std::max(0, std::atoi(argv[6]));
    for (int frame = 0; frame < frames; ++frame) host.step();
  }
  const auto& framebuffer = host.framebuffer();
  std::ofstream output{argv[3], std::ios::binary};
  output << "P4\n" << framebuffer.width() << ' ' << framebuffer.height() << '\n';
  output.write(reinterpret_cast<const char*>(framebuffer.data()),
               static_cast<std::streamsize>(framebuffer.sizeBytes()));
  return output ? 0 : 4;
}
