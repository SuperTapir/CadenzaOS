#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#if defined(__APPLE__)
#include <malloc/malloc.h>
#endif

#include "cadenza/core/input_adapter.h"
#include "cadenza/core/version.h"
#include "cadenza/desktop/desktop_model.h"
#include "cadenza/desktop/png_writer.h"
#include "cadenza/desktop/recording.h"
#include "cadenza/desktop/sdl_audio_output.h"
#include "cadenza/desktop/simulation_controller.h"
#include "cadenza/host/headless_host.h"

namespace {
class SdlClock final : public cadenza::MonotonicClock {
 public:
  cadenza::MonotonicMillis nowMs() const noexcept override {
    return SDL_GetTicks();
  }
};

std::uint64_t heapEstimateBytes() noexcept {
#if defined(__APPLE__)
  malloc_statistics_t statistics{};
  malloc_zone_statistics(malloc_default_zone(), &statistics);
  return statistics.size_in_use;
#else
  return 0;
#endif
}

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
                  std::vector<std::uint8_t>& rgba,
                  cadenza::desktop::DisplayPalette palette) {
  rgba.resize(static_cast<std::size_t>(framebuffer.width()) *
              framebuffer.height() * 4U);
  for (std::int32_t y = 0; y < framebuffer.height(); ++y) {
    for (std::int32_t x = 0; x < framebuffer.width(); ++x) {
      const cadenza::desktop::DisplayColor color =
          cadenza::desktop::displayColor(framebuffer.pixel(x, y), palette);
      const std::size_t offset =
          (static_cast<std::size_t>(y) * framebuffer.width() + x) * 4U;
      rgba[offset] = color.r;
      rgba[offset + 1] = color.g;
      rgba[offset + 2] = color.b;
      rgba[offset + 3] = color.a;
    }
  }
}

struct Options {
  const char* profile = "t-embed";
  int scale = 2;
  int frameLimit = 0;
  cadenza::desktop::DisplayPalette palette =
      cadenza::desktop::DisplayPalette::Reflective;
  bool overlay = false;
  bool deviceFrame = false;
};

bool parseOptions(int argc, char** argv, Options& options) {
  for (int index = 1; index < argc; ++index) {
    if (std::strcmp(argv[index], "--profile") == 0 && index + 1 < argc) {
      options.profile = argv[++index];
    } else if (std::strcmp(argv[index], "--scale") == 0 && index + 1 < argc) {
      options.scale = std::atoi(argv[++index]);
    } else if (std::strcmp(argv[index], "--frames") == 0 && index + 1 < argc) {
      options.frameLimit = std::max(0, std::atoi(argv[++index]));
    } else if (std::strcmp(argv[index], "--palette") == 0 &&
               index + 1 < argc) {
      if (!cadenza::desktop::parseDisplayPalette(options.palette,
                                                  argv[++index])) {
        return false;
      }
    } else if (std::strcmp(argv[index], "--overlay") == 0) {
      options.overlay = true;
    } else if (std::strcmp(argv[index], "--device-frame") == 0) {
      options.deviceFrame = true;
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
                 "[--scale 1..4] [--palette reflective|pure] "
                 "[--overlay] [--device-frame] "
                 "[--frames N]\n");
    return 2;
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
    return 3;
  }

  const auto layout = cadenza::desktop::DevicePreviewLayout::make(
      config.width, config.height, config.scale, options.deviceFrame);
  SDL_Window* window = SDL_CreateWindow(
      "Cadenza OS", layout.windowWidth, layout.windowHeight,
      SDL_WINDOW_HIGH_PIXEL_DENSITY);
  SDL_Renderer* renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
  SDL_Texture* texture = renderer
                             ? SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                                                 SDL_TEXTUREACCESS_STREAMING,
                                                 config.width, config.height)
                             : nullptr;
  if (!window || !renderer || !texture ||
      !SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST) ||
      !SDL_SetRenderLogicalPresentation(
          renderer, layout.logicalWidth, layout.logicalHeight,
          SDL_LOGICAL_PRESENTATION_INTEGER_SCALE)) {
    std::fprintf(stderr, "SDL setup: %s\n", SDL_GetError());
    if (texture) SDL_DestroyTexture(texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
    return 4;
  }

  int backingWidth = 0;
  int backingHeight = 0;
  SDL_GetWindowSizeInPixels(window, &backingWidth, &backingHeight);
  const float pixelDensity = SDL_GetWindowPixelDensity(window);
  std::printf("Cadenza desktop %s (SDL %d.%d.%d), %dx%d @ %dx, %s palette\n",
              cadenza::coreVersion(), SDL_MAJOR_VERSION, SDL_MINOR_VERSION,
              SDL_MICRO_VERSION, config.width, config.height, config.scale,
              options.palette == cadenza::desktop::DisplayPalette::Reflective
                  ? "reflective"
                  : "pure");
  std::printf("Window logical %dx%d, backing %dx%d, pixel density %.2fx\n",
              layout.windowWidth, layout.windowHeight, backingWidth,
              backingHeight, static_cast<double>(pixelDensity));
  std::printf(
      "Connectivity debug: F8 WiFi, F9 link loss, F10 BLE advertise, "
      "F11 provisioning, F12 BLE scan\n");
  cadenza::desktop::DesktopDiagnosticLog diagnostics;
  cadenza::host::HeadlessHost host{config.profile, 1.0F / 60.0F,
                                    &diagnostics};
  cadenza::desktop::SdlAudioOutput audioOutput;
  if (!SDL_InitSubSystem(SDL_INIT_AUDIO) ||
      !audioOutput.start(host.services().sound())) {
    std::fprintf(stderr, "SDL audio disabled: %s\n", SDL_GetError());
  }
  cadenza::desktop::DesktopInputMapper mapper;
  cadenza::InputReducer reducer;
  cadenza::desktop::OverlayState overlay;
  cadenza::desktop::SimulationController simulation;
  cadenza::desktop::RecordingSession recording;
  if (options.overlay) overlay.toggle();
  SdlClock clock;
  std::vector<std::uint8_t> rgba;
  bool running = true;
  bool screenshotRequested = false;
  bool recordingToggleRequested = false;
  bool wifiRequested = false;
  bool bluetoothAdvertisingRequested = false;
  bool bluetoothScanningRequested = false;
  std::uint64_t nextFrameMs = SDL_GetTicks();
  std::uint64_t previousFrameMs = nextFrameMs;
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
        } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                   event.key.key == SDLK_F2) {
          simulation.togglePause();
        } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                   event.key.key == SDLK_F3) {
          simulation.requestSingleStep();
        } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                   event.key.key == SDLK_F4) {
          simulation.toggleStepMode();
        } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                   event.key.key == SDLK_F5) {
          const float speed = simulation.speed();
          simulation.setSpeed(speed == 0.25F ? 0.5F
                              : speed == 0.5F ? 1.0F
                              : speed == 1.0F ? 2.0F
                                              : 0.25F);
        } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                   event.key.key == SDLK_F6) {
          screenshotRequested = true;
        } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                   event.key.key == SDLK_F7) {
          recordingToggleRequested = true;
        } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                   event.key.key == SDLK_F8) {
          wifiRequested = !wifiRequested;
          host.services().submit(
              cadenza::SystemCommand::setNetworkOnlineRequested(
                  wifiRequested));
        } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                   event.key.key == SDLK_F9) {
          const auto& wifi =
              host.services().snapshot().connectivity.wifi;
          if (wifi.generation != 0) {
            host.services().postPlatformEvent(
                cadenza::system::PlatformEvent::wifiDisconnected(
                    wifi.generation, cadenza::WiFiFailure::Driver));
          }
        } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                   event.key.key == SDLK_F10) {
          bluetoothAdvertisingRequested =
              !bluetoothAdvertisingRequested;
          host.services().submit(
              cadenza::SystemCommand::setBluetoothAdvertisingRequested(
                  bluetoothAdvertisingRequested));
        } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                   event.key.key == SDLK_F11) {
          const auto& provisioning =
              host.services().snapshot().connectivity.provisioning;
          if (provisioning.state == cadenza::ProvisioningState::Inactive ||
              provisioning.state == cadenza::ProvisioningState::Failed ||
              provisioning.state == cadenza::ProvisioningState::Succeeded) {
            host.services().submit(
                cadenza::SystemCommand::startProvisioning());
          } else if (provisioning.generation != 0) {
            host.services().postPlatformEvent(
                cadenza::system::PlatformEvent::provisioningProgress(
                    provisioning.generation,
                    cadenza::ProvisioningState::Succeeded));
          }
        } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat &&
                   event.key.key == SDLK_F12) {
          bluetoothScanningRequested = !bluetoothScanningRequested;
          host.services().submit(
              cadenza::SystemCommand::setBluetoothScanningRequested(
                  bluetoothScanningRequested));
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
    const float realDelta = static_cast<float>(nowMs - previousFrameMs) / 1000.0F;
    previousFrameMs = nowMs;
    diagnostics.beginFrame();
    cadenza::pumpInput(mapper, clock, reducer);
    const cadenza::InputFrame input = reducer.takeFrame();
    const float simulationDelta = simulation.consumeDelta(realDelta);
    if (simulationDelta > 0.0F) host.advance(simulationDelta, input);
    if (recordingToggleRequested) {
      if (recording.active()) {
        std::printf("Recording: %s\n", recording.stop() ? "saved" : "failed");
      } else {
        const std::string basename =
            "cadenza-" + std::to_string(static_cast<unsigned long long>(nowMs));
        std::printf("Recording: %s\n",
                    recording.start("recordings", basename, host.framebuffer(),
                                    true)
                        ? "started"
                        : "failed");
      }
      recordingToggleRequested = false;
    }
    if (recording.active() && simulationDelta > 0.0F) {
      if (recording.capture(host.frameIndex(), true, host.framebuffer()) ==
          cadenza::desktop::CaptureResult::Error) {
        std::fprintf(stderr, "Recording capture failed and was cleaned up\n");
      }
    }
    if (screenshotRequested) {
      const std::string filename = cadenza::desktop::screenshotFilename(
          config.width, config.height, host.frameIndex(),
          static_cast<std::uint64_t>(host.simulationSeconds() * 1000.0F));
      if (cadenza::desktop::writePng(filename, host.framebuffer())) {
        std::printf("Screenshot: %s\n", filename.c_str());
      } else {
        std::fprintf(stderr, "Screenshot failed: %s\n", filename.c_str());
      }
      screenshotRequested = false;
    }
    convertFrame(host.framebuffer(), rgba, options.palette);
    SDL_UpdateTexture(texture, nullptr, rgba.data(), config.width * 4);
    SDL_SetRenderDrawColor(renderer, 32, 32, 32, 255);
    SDL_RenderClear(renderer);
    const SDL_FRect destination{static_cast<float>(layout.screenX),
                                static_cast<float>(layout.screenY),
                                static_cast<float>(layout.screenWidth),
                                static_cast<float>(layout.screenHeight)};
    if (options.deviceFrame) {
      SDL_SetRenderDrawColor(renderer, 12, 12, 12, 255);
      const SDL_FRect body{4.0F, 4.0F,
                           static_cast<float>(layout.logicalWidth - 8),
                           static_cast<float>(layout.logicalHeight - 8)};
      SDL_RenderFillRect(renderer, &body);
    }
    SDL_RenderTexture(renderer, texture, nullptr, &destination);

    const float frameMs = static_cast<float>(SDL_GetTicksNS() - frameStart) /
                          1000000.0F;
    const float fps = frameMs > 0.0F ? 1000.0F / frameMs : 0.0F;
    overlay.update(fps, frameMs, host.runtime().currentId(), input);
    overlay.updateResources(host.framebuffer().sizeBytes(),
                            host.framebuffer().capacityBytes(),
                            diagnostics.capacityOverflows(), heapEstimateBytes());
    if (overlay.visible) {
      SDL_SetRenderDrawColor(renderer, 255, 64, 64, 255);
      SDL_RenderDebugTextFormat(
          renderer, 4.0F, 4.0F,
          "FPS %.1f %.2fms APP %s TURN %d P%d R%d C%d L%d H%u %s %.2fx %s",
          overlay.fps,
          overlay.frameMilliseconds,
          host.runtime().appName(overlay.app), overlay.input.turn,
          overlay.input.pressed, overlay.input.released, overlay.input.clicked,
          overlay.input.longPressed, overlay.input.heldMs,
          simulation.paused() ? "PAUSED" : "RUN", simulation.speed(),
          simulation.stepMode() == cadenza::desktop::StepMode::Fixed ? "FIXED"
                                                                      : "REAL");
      SDL_RenderDebugTextFormat(
          renderer, 4.0F, 14.0F, "FB %zu/%zu  OVERFLOW %u  HEAP %llu",
          overlay.framebufferBytes, overlay.framebufferCapacity,
          overlay.capacityOverflows,
          static_cast<unsigned long long>(overlay.heapEstimateBytes));
      const auto& connectivity = host.services().snapshot().connectivity;
      SDL_RenderDebugTextFormat(
          renderer, 4.0F, 24.0F,
          "WIFI %u/%u/%u BLE %u A%d S%d PROV %u/%u",
          static_cast<unsigned>(connectivity.wifi.radio),
          static_cast<unsigned>(connectivity.wifi.station),
          static_cast<unsigned>(connectivity.wifi.failure),
          static_cast<unsigned>(connectivity.bluetoothLe.radio),
          connectivity.bluetoothLe.advertising,
          connectivity.bluetoothLe.scanning,
          static_cast<unsigned>(connectivity.provisioning.state),
          static_cast<unsigned>(connectivity.provisioning.failure));
      if (const auto* diagnostic = diagnostics.recentThisFrame(0)) {
        SDL_RenderDebugTextFormat(
            renderer, 4.0F, 34.0F, "DIAG %u/%u %s (%d)",
            static_cast<unsigned>(diagnostic->category),
            static_cast<unsigned>(diagnostic->code),
            diagnostic->message ? diagnostic->message : "",
            diagnostic->value);
      } else {
        SDL_RenderDebugText(renderer, 4.0F, 34.0F, "DIAG OK");
      }
    }
    SDL_RenderPresent(renderer);
    ++renderedFrames;
    nextFrameMs += 1000U / 60U;
    if (nowMs > nextFrameMs + 100U) nextFrameMs = nowMs;
  }

  if (recording.active()) recording.stop();
  audioOutput.stop();
  SDL_DestroyTexture(texture);
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
