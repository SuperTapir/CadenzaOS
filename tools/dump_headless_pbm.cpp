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
    case 1: return cadenza::apps::kTimerAppId;
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
    for (int frame = 0; frame < 64 && host.runtime().transitioning(); ++frame) {
      host.step();
    }
  }
  if (argc >= 5 && app == cadenza::apps::kTimerAppId &&
      (std::strcmp(argv[4], "running") == 0 ||
       std::strcmp(argv[4], "paused") == 0 ||
       std::strcmp(argv[4], "starting-mid") == 0 ||
       std::strcmp(argv[4], "pausing-mid") == 0 ||
       std::strcmp(argv[4], "resuming-mid") == 0 ||
       std::strcmp(argv[4], "alert") == 0 ||
       std::strcmp(argv[4], "alert-mid") == 0)) {
    cadenza::InputFrame click;
    click.clicked = true;
    host.step(click);
    if (std::strcmp(argv[4], "starting-mid") == 0) {
      host.advance(0.12F);
    } else if (std::strcmp(argv[4], "alert") == 0 ||
               std::strcmp(argv[4], "alert-mid") == 0) {
      host.advance(600.0F);
      if (std::strcmp(argv[4], "alert-mid") == 0) host.advance(0.30F);
    } else {
      host.advance(0.30F);
    }
    if (std::strcmp(argv[4], "paused") == 0 ||
        std::strcmp(argv[4], "pausing-mid") == 0 ||
        std::strcmp(argv[4], "resuming-mid") == 0) {
      host.step(click);
      if (std::strcmp(argv[4], "pausing-mid") == 0) {
        host.advance(0.09F);
      } else {
        host.advance(0.20F);
      }
    }
    if (std::strcmp(argv[4], "resuming-mid") == 0) {
      host.step(click);
      host.advance(0.09F);
    }
  } else if (argc >= 5 && std::strcmp(argv[4], "menu") == 0) {
    cadenza::InputFrame held;
    held.longPressed = true;
    host.step(held);
    cadenza::InputFrame released;
    released.released = true;
    host.step(released);
    for (int frame = 0; frame < 12; ++frame) host.step();
    if (argc >= 6) {
      cadenza::InputFrame turn;
      turn.turn = static_cast<std::int16_t>(std::atoi(argv[5]));
      host.step(turn);
    }
  } else if (argc >= 5 && app == cadenza::apps::kGalleryAppId) {
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
