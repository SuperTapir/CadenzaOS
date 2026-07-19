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
    case 7: return cadenza::apps::kSightAppId;
    default: return cadenza::apps::kLauncherAppId;
  }
}

void renderTypographySpecimen(cadenza::MonoFramebuffer& framebuffer) {
  cadenza::MonoCanvas canvas{framebuffer};
  const bool compact =
      canvas.typography().density == cadenza::TypographyDensity::Compact;
  canvas.clear(false);
  canvas.text(compact ? "TITLE 20 MEDIUM" : "TITLE 24 MEDIUM", 10, 4, 1, true,
              cadenza::TextAlign::TopLeft, cadenza::TextRole::Title);
  canvas.text("BODY 20 MEDIUM", 10, 43, 1, true,
              cadenza::TextAlign::TopLeft, cadenza::TextRole::Body);
  canvas.text(compact ? "COMPACT 10 BOLD" : "COMPACT 11 BOLD", 10, 79, 1, true,
              cadenza::TextAlign::TopLeft, cadenza::TextRole::Compact);
  cadenza::BoundedTextRequest longLabel;
  longLabel.value = "CAPTION LONG SETTINGS LABEL FOR ELLIPSIS";
  longLabel.bounds = {10, 103, framebuffer.width() - 20, 22};
  longLabel.align = cadenza::TextAlign::MiddleLeft;
  longLabel.overflow = cadenza::TextOverflowPolicy::Ellipsis;
  longLabel.role = cadenza::TextRole::Caption;
  canvas.boundedText(longLabel, true);
  const cadenza::Rect selected{10, framebuffer.height() - 32,
                               framebuffer.width() - 20, 26};
  canvas.fillRect(selected.x, selected.y, selected.width, selected.height,
                  true);
  canvas.text("SELECTED COMPACT", selected.x + 8,
              selected.y + selected.height / 2, 1, false,
              cadenza::TextAlign::MiddleLeft,
              cadenza::TextRole::Compact);
}

void renderOverlaySpecimen(cadenza::MonoFramebuffer& framebuffer) {
  cadenza::MonoCanvas canvas{framebuffer};
  canvas.clear(false);
  for (int y = 0; y < framebuffer.height(); y += 16) {
    canvas.line(0, y, framebuffer.width() - 1, y, true);
  }
  cadenza::presentation::SystemSurfaceCoordinator surfaces;
  surfaces.pushTransient("SAVED", 1.0F);
  cadenza::presentation::renderTransientFeedback(canvas, surfaces);
  cadenza::presentation::SystemUi::statusIndicator(
      canvas, {framebuffer.width() - 82, 4, 78, 26}, "SYNC", true);
}

void settleTransition(cadenza::host::HeadlessHost& host) {
  for (int frame = 0; frame < 64 && host.runtime().transitioning(); ++frame) {
    host.step();
  }
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 4 || argc > 7) return 2;
  cadenza::host::HeadlessHost host{profileFrom(argv[1])};
  const bool typographySpecimen = std::atoi(argv[2]) == 5;
  const bool overlaySpecimen = std::atoi(argv[2]) == 6;
  const cadenza::AppId app = appFrom(argv[2]);
  const bool pausedBackgroundTimer =
      !typographySpecimen && !overlaySpecimen && argc >= 5 &&
      std::strcmp(argv[4], "background-timer-paused") == 0;
  const bool backgroundTimer =
      !typographySpecimen && !overlaySpecimen && argc >= 5 &&
      (std::strcmp(argv[4], "background-timer") == 0 ||
       pausedBackgroundTimer);
  if (typographySpecimen) {
    renderTypographySpecimen(host.framebuffer());
  } else if (overlaySpecimen) {
    renderOverlaySpecimen(host.framebuffer());
  } else if (backgroundTimer) {
    if (app == cadenza::apps::kTimerAppId ||
        !host.runtime().open(cadenza::apps::kTimerAppId)) {
      return 3;
    }
    settleTransition(host);
    cadenza::InputFrame click;
    click.clicked = true;
    host.step(click);
    host.advance(0.30F);
    if (pausedBackgroundTimer) {
      host.step(click);
      host.advance(0.20F);
    }
    if (!host.runtime().open(app)) return 3;
    settleTransition(host);
  } else if (app != cadenza::apps::kLauncherAppId) {
    if (!host.runtime().open(app)) return 3;
    settleTransition(host);
  }
  if (!backgroundTimer && !typographySpecimen && !overlaySpecimen &&
      argc >= 5 &&
      app == cadenza::apps::kTimerAppId &&
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
  } else if (!typographySpecimen && !overlaySpecimen && argc >= 5 &&
             std::strcmp(argv[4], "menu") == 0) {
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
  } else if (!backgroundTimer && !typographySpecimen && !overlaySpecimen &&
             argc >= 5 && app == cadenza::apps::kSightAppId &&
             (std::strcmp(argv[4], "question") == 0 ||
              std::strcmp(argv[4], "note-entry-mid") == 0 ||
              std::strcmp(argv[4], "page-enter-mid") == 0 ||
              std::strcmp(argv[4], "page-return-mid") == 0 ||
              std::strcmp(argv[4], "answer") == 0 ||
              std::strcmp(argv[4], "chord-answer") == 0 ||
              std::strcmp(argv[4], "chord-answer-mid") == 0 ||
              std::strcmp(argv[4], "chord-exit-mid") == 0)) {
    const bool chordAnswer =
        std::strcmp(argv[4], "chord-answer") == 0 ||
        std::strcmp(argv[4], "chord-answer-mid") == 0 ||
        std::strcmp(argv[4], "chord-exit-mid") == 0;
    if (chordAnswer) {
      cadenza::InputFrame selectChords;
      selectChords.turn = 2;
      host.step(selectChords);
    }
    cadenza::InputFrame click;
    click.clicked = true;
    host.step(click);
    if (std::strcmp(argv[4], "note-entry-mid") == 0) {
      host.advance(cadenza::SightApp::kNoteEntranceSeconds / 2.0F);
    } else if (std::strcmp(argv[4], "page-enter-mid") == 0) {
      host.advance(cadenza::SightApp::kPageTransitionSeconds / 2.0F);
    } else if (std::strcmp(argv[4], "page-return-mid") == 0) {
      host.advance(cadenza::SightApp::kPageTransitionSeconds);
      host.step(click);
      host.advance(cadenza::SightApp::kAnswerRevealSeconds);
      cadenza::InputFrame selectLevel;
      selectLevel.turn = 1;
      host.step(selectLevel);
      host.step(click);
      host.advance(cadenza::SightApp::kPageTransitionSeconds / 2.0F);
    }
    if (std::strcmp(argv[4], "answer") == 0 || chordAnswer) {
      host.step(click);
      const bool answerMid =
          std::strcmp(argv[4], "chord-answer-mid") == 0;
      host.advance(answerMid ? cadenza::SightApp::kAnswerRevealSeconds / 2.0F
                             : cadenza::SightApp::kAnswerRevealSeconds);
      if (std::strcmp(argv[4], "chord-exit-mid") == 0) {
        host.step(click);
        host.advance(cadenza::SightApp::kAnswerRevealSeconds / 2.0F);
      }
    }
  } else if (!backgroundTimer && !typographySpecimen && !overlaySpecimen &&
             argc >= 5 && app == cadenza::apps::kSettingsAppId &&
             std::strcmp(argv[4], "about") == 0) {
    cadenza::InputFrame selectAbout;
    selectAbout.turn = 5;
    host.step(selectAbout);
    cadenza::InputFrame click;
    click.clicked = true;
    host.step(click);
  } else if (!backgroundTimer && !typographySpecimen && !overlaySpecimen &&
             argc >= 5 &&
             app == cadenza::apps::kGalleryAppId) {
    cadenza::InputFrame input;
    input.turn = static_cast<std::int16_t>(std::atoi(argv[4]));
    host.step(input);
  }
  if (!backgroundTimer && !typographySpecimen && !overlaySpecimen &&
      argc == 6 &&
      app == cadenza::apps::kGalleryAppId) {
    cadenza::InputFrame scrubMode;
    scrubMode.clicked = true;
    host.step(scrubMode);
    cadenza::InputFrame scrub;
    scrub.turn = static_cast<std::int16_t>(std::atoi(argv[5]));
    host.step(scrub);
  }
  if (!backgroundTimer && !typographySpecimen && !overlaySpecimen &&
      argc == 7 &&
      app == cadenza::apps::kGalleryAppId) {
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
