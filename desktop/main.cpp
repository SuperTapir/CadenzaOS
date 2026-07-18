#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "cadenza/core/input_adapter.h"
#include "cadenza/core/version.h"
#include "cadenza/desktop/desktop_model.h"
#include "cadenza/host/headless_host.h"

namespace {
class SdlClock final : public cadenza::MonotonicClock {
 public:
  cadenza::MonotonicMillis nowMs() const noexcept override {
    return SDL_GetTicks();
  }
};

cadenza::desktop::DesktopKey mapKey(SDL_Keycode key) {
  switch (key) {
    case SDLK_LEFT:
      return cadenza::desktop::DesktopKey::Left;
    case SDLK_RIGHT:
      return cadenza::desktop::DesktopKey::Right;
    case SDLK_SPACE:
      return cadenza::desktop::DesktopKey::Space;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
      return cadenza::desktop::DesktopKey::Enter;
    default:
      return cadenza::desktop::DesktopKey::Unknown;
  }
}

void convertFrame(const cadenza::MonoFramebuffer& framebuffer,
                  std::vector<std::uint8_t>& rgba) {
  rgba.resize(static_cast<std::size_t>(framebuffer.width()) *
              framebuffer.height() * 4U);
  for (std::int32_t y = 0; y < framebuffer.height(); ++y) {
    for (std::int32_t x = 0; x < framebuffer.width(); ++x) {
      const std::uint8_t value = framebuffer.pixel(x, y) ? 0 : 255;
      const std::size_t offset =
          (static_cast<std::size_t>(y) * framebuffer.width() + x) * 4U;
      rgba[offset] = value;
      rgba[offset + 1] = value;
      rgba[offset + 2] = value;
      rgba[offset + 3] = 255;
    }
  }
}

struct Options {
  const char* profile = "t-embed";
  int scale = 2;
  int frameLimit = 0;
  bool overlay = false;
};

bool parseOptions(int argc, char** argv, Options& options) {
  for (int index = 1; index < argc; ++index) {
    if (std::strcmp(argv[index], "--profile") == 0 && index + 1 < argc) {
      options.profile = argv[++index];
    } else if (std::strcmp(argv[index], "--scale") == 0 && index + 1 < argc) {
      options.scale = std::atoi(argv[++index]);
    } else if (std::strcmp(argv[index], "--frames") == 0 && index + 1 < argc) {
      options.frameLimit = std::max(0, std::atoi(argv[++index]));
    } else if (std::strcmp(argv[index], "--overlay") == 0) {
      options.overlay = true;
    } else {
      return false;
    }
  }
  return true;
}
}  // namespace

int main(int argc, char** argv) {
  Options options;
  cadenza::desktop::DesktopConfig config;
  if (!parseOptions(argc, argv, options) ||
      !cadenza::desktop::configure(config, options.profile, options.scale)) {
    std::fprintf(stderr,
                 "usage: cadenza_desktop [--profile t-embed|sharp] "
                 "[--scale 1..4] [--overlay] [--frames N]\n");
    return 2;
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 3;
  }

  SDL_Window* window = SDL_CreateWindow(
      "Cadenza OS", config.width * config.scale, config.height * config.scale,
      0);
  SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
  SDL_Texture* texture = renderer
                             ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                                 SDL_TEXTUREACCESS_STREAMING,
                                                 config.width, config.height)
                             : nullptr;
  if (!window || !renderer || !texture ||
      !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST) ||
      !SDL_SetRenderLogicalPresentation(
          renderer, config.width, config.height,
          SDL_LOGICAL_PRESENTATION_INTEGER_SCALE)) {
    std::fprintf(stderr, "SDL setup: %s\n", SDL_GetError());
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return 4;
  }

  std::printf("Cadenza desktop %s (SDL %d.%d.%d), %dx%d @ %dx\n",
              cadenza::coreVersion(), SDL_MAJOR_VERSION, SDL_MINOR_VERSION,
              SDL_MICRO_VERSION, config.width, config.height, config.scale);
  cadenza::host::HeadlessHost host{config.profile};
  cadenza::desktop::DesktopInputMapper mapper;
  cadenza::InputReducer reducer;
  cadenza::desktop::OverlayState overlay;
  if (options.overlay) overlay.toggle();
  SdlClock clock;
  std::vector<std::uint8_t> rgba;
  bool running = true;
  std::uint64_t nextFrameMs = SDL_GetTicks();
  int renderedFrames = 0;

  while (running && (options.frameLimit == 0 || renderedFrames < options.frameLimit)) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      const auto timestamp = static_cast<cadenza::MonotonicMillis>(SDL_GetTicks());
      if (event.type == SDL_EVENT_QUIT) {
        running = false;
      } else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
        mapper.wheel(event.wheel.y, timestamp);
      } else if (event.type == SDL_EVENT_KEY_DOWN ||
                 event.type == SDL_EVENT_KEY_UP) {
        if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_F1 &&
            !event.key.repeat) {
          overlay.toggle();
        } else {
          mapper.key(mapKey(event.key.key), event.type == SDL_EVENT_KEY_DOWN,
                     event.key.repeat, timestamp);
        }
      }
    }

    const std::uint64_t nowMs = SDL_GetTicks();
    if (nowMs < nextFrameMs) {
      SDL_Delay(1);
      continue;
    }
    const std::uint64_t frameStart = SDL_GetTicksNS();
    cadenza::pumpInput(mapper, clock, reducer);
    const cadenza::InputFrame input = reducer.takeFrame();
    host.step(input);
    convertFrame(host.framebuffer(), rgba);
    SDL_UpdateTexture(texture, nullptr, rgba.data(), config.width * 4);
    SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, texture, nullptr, nullptr);

    const float frameMs = static_cast<float>(SDL_GetTicksNS() - frameStart) /
                          1000000.0F;
    const float fps = frameMs > 0.0F ? 1000.0F / frameMs : 0.0F;
    overlay.update(fps, frameMs, host.runtime().currentId(), input);
    if (overlay.visible) {
      SDL_SetRenderDrawColor(renderer, 255, 64, 64, 255);
      SDL_RenderDebugTextFormat(
          renderer, 4.0F, 4.0F,
          "FPS %.1f  %.2fms  APP %s  TURN %d  P%d R%d C%d L%d H%u", overlay.fps,
          overlay.frameMilliseconds,
          host.runtime().appName(overlay.app), overlay.input.turn,
          overlay.input.pressed, overlay.input.released, overlay.input.clicked,
          overlay.input.longPressed, overlay.input.heldMs);
    }
    SDL_RenderPresent(renderer);
    ++renderedFrames;
    nextFrameMs += 1000U / 60U;
    if (nowMs > nextFrameMs + 100U) nextFrameMs = nowMs;
  }

  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
