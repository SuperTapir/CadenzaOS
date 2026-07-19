#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>

#include "cadenza/core/mono_canvas.h"
#include "cadenza/presentation/system_surface.h"

namespace {

std::uint64_t framebufferHash(const cadenza::MonoFramebuffer& frame) {
  std::uint64_t hash = 1469598103934665603ULL;
  for (std::size_t index = 0; index < frame.sizeBytes(); ++index) {
    hash = (hash ^ frame.data()[index]) * 1099511628211ULL;
  }
  return hash;
}

cadenza::InputFrame longPress() {
  cadenza::InputFrame input;
  input.longPressed = true;
  input.heldMs = 650;
  return input;
}

cadenza::InputFrame release() {
  cadenza::InputFrame input;
  input.released = true;
  return input;
}

class GeometryDiagnosticSink final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    if (event.code == cadenza::DiagnosticCode::InvalidGeometry ||
        event.code == cadenza::DiagnosticCode::ClippedGeometry ||
        event.code == cadenza::DiagnosticCode::FullyClipped) {
      ++geometryFailures;
    }
  }

  int geometryFailures = 0;
};

}  // namespace

TEST_CASE("long press sequence arms menu only after trigger release") {
  cadenza::presentation::SystemSurfaceCoordinator surfaces;
  const auto opened = surfaces.update(0.01F, longPress(), false);
  CHECK(opened.consumeInput);
  CHECK(opened.captureFrozenFrame);
  CHECK(opened.intent == cadenza::presentation::SystemSurfaceIntent::Opened);
  REQUIRE(surfaces.menuActive());
  CHECK(surfaces.revealProgress() == doctest::Approx(0.0F));

  cadenza::InputFrame held;
  held.turn = 2;
  held.clicked = true;
  const auto heldFrame = surfaces.update(0.01F, held, false);
  CHECK(heldFrame.consumeInput);
  CHECK(heldFrame.intent == cadenza::presentation::SystemSurfaceIntent::None);
  CHECK(surfaces.selection() == cadenza::presentation::SystemMenuItem::Resume);

  const auto released = surfaces.update(0.01F, release(), false);
  CHECK(released.consumeInput);
  CHECK(surfaces.menuActive());
  CHECK(surfaces.selection() == cadenza::presentation::SystemMenuItem::Resume);
  CHECK(surfaces.revealProgress() > 0.0F);
  surfaces.update(0.2F, {}, false);
  CHECK(surfaces.revealProgress() == doctest::Approx(1.0F));

  cadenza::InputFrame turn;
  turn.turn = 1;
  const auto navigated = surfaces.update(0.01F, turn, false);
  CHECK(navigated.intent ==
        cadenza::presentation::SystemSurfaceIntent::Navigate);
  CHECK(surfaces.selection() == cadenza::presentation::SystemMenuItem::Home);
}

TEST_CASE("long press close consumes held sequence through release") {
  cadenza::presentation::SystemSurfaceCoordinator surfaces;
  surfaces.update(0.01F, longPress(), false);
  surfaces.update(0.01F, release(), false);
  surfaces.update(0.20F, {}, false);
  REQUIRE(surfaces.menuActive());

  const auto closed = surfaces.update(0.01F, longPress(), false);
  CHECK(closed.intent == cadenza::presentation::SystemSurfaceIntent::Closed);
  CHECK(surfaces.menuActive());
  CHECK(surfaces.menuClosing());
  CHECK(surfaces.appSuspended());

  cadenza::InputFrame noisyRelease = release();
  noisyRelease.clicked = true;
  noisyRelease.turn = 3;
  const auto captured = surfaces.update(0.01F, noisyRelease, false);
  CHECK(captured.consumeInput);
  CHECK(captured.intent == cadenza::presentation::SystemSurfaceIntent::None);
  CHECK(surfaces.appSuspended());

  const float before = surfaces.revealProgress();
  cadenza::InputFrame ignored;
  ignored.turn = 2;
  ignored.clicked = true;
  CHECK(surfaces.update(0.05F, ignored, false).consumeInput);
  CHECK(surfaces.menuClosing());
  CHECK(surfaces.revealProgress() < before);
  CHECK(surfaces.selection() == cadenza::presentation::SystemMenuItem::Resume);

  surfaces.update(0.30F, {}, false);
  CHECK_FALSE(surfaces.menuActive());
  CHECK_FALSE(surfaces.menuClosing());
  CHECK_FALSE(surfaces.appSuspended());

  cadenza::InputFrame next;
  next.clicked = true;
  CHECK_FALSE(surfaces.update(0.01F, next, false).consumeInput);
}

TEST_CASE("menu closing progress is monotonic and releases only offscreen") {
  cadenza::presentation::SystemSurfaceCoordinator surfaces;
  surfaces.update(0.01F, longPress(), false);
  surfaces.update(0.01F, release(), false);
  surfaces.update(0.20F, {}, false);
  REQUIRE(surfaces.revealProgress() == doctest::Approx(1.0F));

  cadenza::InputFrame click;
  click.clicked = true;
  const auto closing = surfaces.update(0.0F, click, false);
  CHECK(closing.intent ==
        cadenza::presentation::SystemSurfaceIntent::Closed);
  REQUIRE(surfaces.menuClosing());
  float previous = surfaces.revealProgress();
  for (int frame = 0; frame < 20 && surfaces.menuActive(); ++frame) {
    const auto owned = surfaces.update(1.0F / 60.0F, {}, false);
    CHECK(owned.consumeInput);
    CHECK(surfaces.revealProgress() <= previous);
    previous = surfaces.revealProgress();
  }
  CHECK_FALSE(surfaces.menuActive());
  CHECK(previous == doctest::Approx(0.0F));
  CHECK_FALSE(surfaces.update(0.01F, {}, false).consumeInput);
}

TEST_CASE("normal menu reveal keeps the reference two-tenths cadence") {
  cadenza::presentation::SystemSurfaceCoordinator surfaces;
  REQUIRE(surfaces.requestInteractive(
      cadenza::presentation::SurfaceKind::SystemMenu));
  surfaces.update(0.16F, {}, false);
  CHECK(surfaces.revealProgress() == doctest::Approx(0.80F));
  surfaces.update(0.04F, {}, false);
  CHECK(surfaces.revealProgress() == doctest::Approx(1.0F));
}

TEST_CASE("Home menu omits Home from its navigation order") {
  cadenza::presentation::SystemSurfaceCoordinator surfaces;
  surfaces.update(0.01F, longPress(), false, true);
  surfaces.update(0.01F, release(), false, true);
  REQUIRE(surfaces.menuActive());

  cadenza::InputFrame turn;
  turn.turn = 1;
  CHECK(surfaces.update(0.01F, turn, false, true).intent ==
        cadenza::presentation::SystemSurfaceIntent::Navigate);
  CHECK(surfaces.selection() == cadenza::presentation::SystemMenuItem::Sound);

  turn.turn = 8;
  surfaces.update(0.01F, turn, false, true);
  CHECK(surfaces.selection() == cadenza::presentation::SystemMenuItem::Motion);
}

TEST_CASE("transition menu request is captured and deferred to stable frame") {
  cadenza::presentation::SystemSurfaceCoordinator surfaces;
  const auto requested = surfaces.update(0.01F, longPress(), true);
  CHECK(requested.consumeInput);
  CHECK(surfaces.hasDeferredMenu());
  CHECK_FALSE(surfaces.menuActive());
  surfaces.update(0.01F, release(), true);
  surfaces.notifyTransitionStable();
  const auto opened = surfaces.update(0.01F, {}, false);
  CHECK(opened.intent == cadenza::presentation::SystemSurfaceIntent::Opened);
  CHECK(opened.captureFrozenFrame);
  CHECK(surfaces.menuActive());
}

TEST_CASE("Timer alert preempts Menu and owns the acknowledgement click") {
  using namespace cadenza::presentation;
  SystemSurfaceCoordinator surfaces;
  cadenza::TimerSnapshot ready;
  cadenza::TimerSnapshot expired;
  expired.state = cadenza::TimerState::Expired;
  expired.remainingMs = 0;
  expired.expirationGeneration = 1;

  surfaces.update(0.01F, longPress(), false, false,
                  cadenza::MotionProfile::Normal, ready);
  surfaces.update(0.01F, release(), false, false,
                  cadenza::MotionProfile::Normal, ready);
  REQUIRE(surfaces.menuActive());

  const auto alert = surfaces.update(0.01F, {}, false, false,
                                     cadenza::MotionProfile::Normal, expired);
  CHECK(alert.consumeInput);
  CHECK(alert.captureFrozenFrame);
  CHECK(surfaces.timerAlertActive());
  CHECK(surfaces.timerAlertElapsed() == doctest::Approx(0.0F));
  CHECK_FALSE(surfaces.menuActive());

  surfaces.update(0.25F, {}, false, false,
                  cadenza::MotionProfile::Normal, expired);
  CHECK(surfaces.timerAlertElapsed() == doctest::Approx(0.25F));

  cadenza::InputFrame click;
  click.clicked = true;
  click.released = true;
  const auto acknowledged = surfaces.update(
      0.01F, click, false, false, cadenza::MotionProfile::Normal, expired);
  CHECK(acknowledged.intent == SystemSurfaceIntent::TimerAcknowledge);
  CHECK(surfaces.timerAlertActive());
  CHECK(surfaces.timerAlertElapsed() == doctest::Approx(0.0F));
}

TEST_CASE("Button sequence held at expiry cannot acknowledge Timer alert") {
  using namespace cadenza::presentation;
  SystemSurfaceCoordinator surfaces;
  cadenza::TimerSnapshot ready;
  cadenza::TimerSnapshot expired;
  expired.state = cadenza::TimerState::Expired;
  expired.expirationGeneration = 4;

  cadenza::InputFrame pressed;
  pressed.pressed = true;
  pressed.heldMs = 10;
  surfaces.update(0.01F, pressed, false, false,
                  cadenza::MotionProfile::Normal, ready);
  cadenza::InputFrame held;
  held.heldMs = 700;
  const auto opened = surfaces.update(
      0.01F, held, false, false, cadenza::MotionProfile::Normal, expired);
  CHECK(opened.consumeInput);
  REQUIRE(surfaces.timerAlertActive());

  cadenza::InputFrame oldRelease;
  oldRelease.released = true;
  oldRelease.clicked = true;
  CHECK(surfaces.update(0.01F, oldRelease, false, false,
                        cadenza::MotionProfile::Normal, expired)
            .intent == SystemSurfaceIntent::None);

  cadenza::InputFrame freshClick;
  freshClick.clicked = true;
  freshClick.released = true;
  CHECK(surfaces.update(0.01F, freshClick, false, false,
                        cadenza::MotionProfile::Normal, expired)
            .intent == SystemSurfaceIntent::TimerAcknowledge);
}

TEST_CASE("Timer alert renderer loops and adapts to Motion Profile") {
  for (const auto profile : {cadenza::FramebufferProfile::TEmbed,
                             cadenza::FramebufferProfile::Sharp}) {
    cadenza::MonoFramebuffer first{profile};
    cadenza::MonoFramebuffer second{profile};
    cadenza::MonoFramebuffer midpoint{profile};
    cadenza::MonoFramebuffer reducedFirst{profile};
    cadenza::MonoFramebuffer reducedSecond{profile};
    first.clear(true);
    second.clear(true);
    midpoint.clear(true);
    reducedFirst.clear(true);
    reducedSecond.clear(true);
    cadenza::MonoCanvas firstCanvas{first};
    cadenza::MonoCanvas secondCanvas{second};
    cadenza::MonoCanvas midpointCanvas{midpoint};
    cadenza::MonoCanvas reducedFirstCanvas{reducedFirst};
    cadenza::MonoCanvas reducedSecondCanvas{reducedSecond};
    cadenza::TimerSnapshot timer;
    cadenza::presentation::renderTimerAlert(
        firstCanvas, timer, 0.0F, cadenza::MotionProfile::Normal);
    cadenza::presentation::renderTimerAlert(
        secondCanvas, timer, 1.2F, cadenza::MotionProfile::Normal);
    cadenza::presentation::renderTimerAlert(
        midpointCanvas, timer, 0.6F, cadenza::MotionProfile::Normal);
    cadenza::presentation::renderTimerAlert(
        reducedFirstCanvas, timer, 0.0F, cadenza::MotionProfile::Reduced);
    cadenza::presentation::renderTimerAlert(
        reducedSecondCanvas, timer, 0.6F, cadenza::MotionProfile::Reduced);
    CHECK(framebufferHash(first) == framebufferHash(second));
    CHECK(framebufferHash(first) != framebufferHash(midpoint));
    CHECK(framebufferHash(reducedFirst) == framebufferHash(reducedSecond));
    CHECK(first.pixel(0, 0));
    CHECK_FALSE(first.pixel(13, 13));
  }
}

TEST_CASE("Timer alert hero and measured action stay inside both viewports") {
  for (const auto profile : {cadenza::FramebufferProfile::TEmbed,
                             cadenza::FramebufferProfile::Sharp}) {
    cadenza::MonoFramebuffer frame{profile};
    GeometryDiagnosticSink diagnostics;
    cadenza::MonoCanvas canvas{frame, &diagnostics};
    cadenza::TimerSnapshot timer;
    timer.configuredDurationMs = 10U * 60U * 1000U;
    cadenza::presentation::renderTimerAlert(
        canvas, timer, 0.0F, cadenza::MotionProfile::Reduced);
    CHECK(diagnostics.geometryFailures == 0);
    CHECK(canvas.measureText("TIME UP", cadenza::TextRole::Hero).height == 36);
  }
}

TEST_CASE("interactive and transient capacities reject deterministically") {
  using namespace cadenza::presentation;
  SystemSurfaceCoordinator surfaces;
  REQUIRE(surfaces.requestInteractive(SurfaceKind::SystemMenu));
  CHECK_FALSE(surfaces.requestInteractive(SurfaceKind::SystemMenu));
  CHECK(surfaces.diagnostics().lastRejection == SurfaceRejection::Busy);
  CHECK_FALSE(surfaces.requestInteractive(SurfaceKind::Dialog));
  CHECK(surfaces.diagnostics().lastRejection == SurfaceRejection::DeniedOwner);
  CHECK_FALSE(surfaces.rejectInvalidAction());
  CHECK(surfaces.diagnostics().lastRejection ==
        SurfaceRejection::InvalidAction);

  for (std::size_t index = 0; index < surfaces.kTransientCapacity; ++index) {
    REQUIRE(surfaces.pushTransient("SAVED", 1.0F));
  }
  CHECK_FALSE(surfaces.pushTransient("OVERFLOW", 1.0F));
  CHECK(surfaces.diagnostics().lastRejection ==
        SurfaceRejection::TransientQueueFull);
  CHECK(surfaces.diagnostics().transientHighWater ==
        surfaces.kTransientCapacity);
  REQUIRE(surfaces.requestInteractive(SurfaceKind::None));
  CHECK(surfaces.update(0.01F, {}, false).consumeInput);
  surfaces.update(0.20F, {}, false);
  CHECK_FALSE(surfaces.update(0.01F, {}, false).consumeInput);
  surfaces.update(2.0F, {}, false);
  CHECK(surfaces.transientCount() == 0);
}

TEST_CASE("system menu render is deterministic for both framebuffer profiles") {
  const cadenza::FramebufferProfile profiles[] = {
      cadenza::FramebufferProfile::TEmbed,
      cadenza::FramebufferProfile::Sharp};
  const std::uint64_t goldenHashes[] = {13913091511711909422ULL,
                                        6718633362590459750ULL};
  for (std::size_t profileIndex = 0; profileIndex < 2; ++profileIndex) {
    const auto profile = profiles[profileIndex];
    cadenza::MonoFramebuffer first{profile};
    cadenza::MonoFramebuffer second{profile};
    cadenza::MonoCanvas firstCanvas{first};
    cadenza::MonoCanvas secondCanvas{second};
    cadenza::SystemSnapshot snapshot;
    snapshot.soundVolume = cadenza::audio::SoundVolume::Muted;
    snapshot.motionProfile = cadenza::MotionProfile::Reduced;
    cadenza::presentation::renderSystemMenu(
        firstCanvas, cadenza::presentation::SystemMenuItem::Resume, true,
        snapshot);
    cadenza::presentation::renderSystemMenu(
        secondCanvas, cadenza::presentation::SystemMenuItem::Resume, true,
        snapshot);
    CHECK(framebufferHash(first) == framebufferHash(second));
    CHECK(framebufferHash(first) == goldenHashes[profileIndex]);
    const auto layout = cadenza::presentation::SystemMenuLayout::forCanvas(
        first.width(), first.height());
    CHECK(layout.panel.x > 0);
    CHECK(layout.panel.x + layout.panel.width == first.width());
    CHECK(layout.panel.x >= first.width() / 3);
    CHECK(first.pixel(layout.panel.x, layout.panel.y));
  }
}

TEST_CASE("system menu mask preserves the frozen app instead of replacing it") {
  cadenza::MonoFramebuffer frame{cadenza::FramebufferProfile::TEmbed};
  frame.clear(true);
  cadenza::MonoCanvas canvas{frame};
  cadenza::SystemSnapshot snapshot;
  cadenza::presentation::renderSystemMenu(
      canvas, cadenza::presentation::SystemMenuItem::Resume, false, snapshot);
  const auto layout = cadenza::presentation::SystemMenuLayout::forCanvas(
      frame.width(), frame.height());
  for (std::int32_t y = 0; y < frame.height(); y += 7) {
    for (std::int32_t x = 0; x < layout.panel.x; x += 7) {
      CHECK(frame.pixel(x, y));
    }
  }
}

TEST_CASE("warped menu is stable when open and deforms deterministically") {
  for (const auto profile : {cadenza::FramebufferProfile::TEmbed,
                             cadenza::FramebufferProfile::Sharp}) {
    cadenza::MonoFramebuffer rigid{profile};
    cadenza::MonoFramebuffer stable{profile};
    cadenza::MonoFramebuffer openingFirst{profile};
    cadenza::MonoFramebuffer openingSecond{profile};
    cadenza::MonoFramebuffer openingEarly{profile};
    cadenza::MonoFramebuffer closing{profile};
    cadenza::MonoFramebuffer hidden{profile};
    cadenza::MonoFramebuffer scratch{profile};
    rigid.clear(false);
    stable.clear(false);
    openingFirst.clear(false);
    openingSecond.clear(false);
    openingEarly.clear(false);
    closing.clear(false);
    hidden.clear(false);
    cadenza::MonoCanvas rigidCanvas{rigid};
    cadenza::MonoCanvas stableCanvas{stable};
    cadenza::MonoCanvas openingFirstCanvas{openingFirst};
    cadenza::MonoCanvas openingSecondCanvas{openingSecond};
    cadenza::MonoCanvas openingEarlyCanvas{openingEarly};
    cadenza::MonoCanvas closingCanvas{closing};
    cadenza::MonoCanvas hiddenCanvas{hidden};
    cadenza::SystemSnapshot snapshot;

    cadenza::presentation::renderSystemMenu(
        rigidCanvas, cadenza::presentation::SystemMenuItem::Sound, false,
        snapshot, 1.0F);
    cadenza::presentation::renderSystemMenu(
        stableCanvas, scratch, cadenza::presentation::SystemMenuItem::Sound,
        false, snapshot, 1.0F, false);
    CHECK(framebufferHash(rigid) == framebufferHash(stable));

    cadenza::presentation::renderSystemMenu(
        hiddenCanvas, scratch, cadenza::presentation::SystemMenuItem::Sound,
        false, snapshot, 0.0F, false);
    cadenza::MonoFramebuffer frozenBaseline{profile};
    frozenBaseline.clear(false);
    CHECK(framebufferHash(hidden) == framebufferHash(frozenBaseline));

    cadenza::presentation::renderSystemMenu(
        openingEarlyCanvas, scratch,
        cadenza::presentation::SystemMenuItem::Sound, false, snapshot,
        0.15F, false);
    cadenza::presentation::renderSystemMenu(
        openingFirstCanvas, scratch,
        cadenza::presentation::SystemMenuItem::Sound, false, snapshot,
        0.35F, false);
    cadenza::presentation::renderSystemMenu(
        openingSecondCanvas, scratch,
        cadenza::presentation::SystemMenuItem::Sound, false, snapshot,
        0.35F, false);
    cadenza::presentation::renderSystemMenu(
        closingCanvas, scratch, cadenza::presentation::SystemMenuItem::Sound,
        false, snapshot, 0.35F, true);
    CHECK(framebufferHash(openingFirst) == framebufferHash(openingSecond));
    CHECK(framebufferHash(openingFirst) != framebufferHash(closing));
    CHECK(framebufferHash(openingFirst) != framebufferHash(stable));

    const auto layout = cadenza::presentation::SystemMenuLayout::forCanvas(
        openingFirst.width(), openingFirst.height());

    // The reference Menu behaves like a sheet pulled from its top-left
    // corner: the top scanline lands immediately while opening, then peels
    // away immediately while closing. Opening and closing are directional
    // sweeps, not the same geometry played forwards and backwards.
    cadenza::MonoFramebuffer openingFirstStep{profile};
    cadenza::MonoFramebuffer closingFirstStep{profile};
    cadenza::MonoCanvas openingFirstStepCanvas{openingFirstStep};
    cadenza::MonoCanvas closingFirstStepCanvas{closingFirstStep};
    cadenza::presentation::renderSystemMenu(
        openingFirstStepCanvas, scratch,
        cadenza::presentation::SystemMenuItem::Sound, false, snapshot,
        0.01F, false);
    cadenza::presentation::renderSystemMenu(
        closingFirstStepCanvas, scratch,
        cadenza::presentation::SystemMenuItem::Sound, false, snapshot,
        0.99F, true);
    CHECK(openingFirstStep.pixel(layout.panel.x + 1, layout.panel.y));
    CHECK_FALSE(closingFirstStep.pixel(layout.panel.x + 1,
                                       layout.panel.y));

    // At 24 FPS a 200 ms reveal advances by roughly 0.208 on its first
    // visible frame. The reference still has the bottom edge pinned to the
    // right side at that instant; without the bottom lead-in delay it is
    // already about half open and the warp loses most of its amplitude.
    cadenza::MonoFramebuffer opening24FpsFirst{profile};
    cadenza::MonoCanvas opening24FpsFirstCanvas{opening24FpsFirst};
    cadenza::presentation::renderSystemMenu(
        opening24FpsFirstCanvas, scratch,
        cadenza::presentation::SystemMenuItem::Sound, false, snapshot,
        (1.0F / 24.0F) / 0.20F, false);
    constexpr float kFirst24FpsProgress = (1.0F / 24.0F) / 0.20F;
    const float oldRemaining = 1.0F - kFirst24FpsProgress;
    const float oldBottomReveal =
        1.0F - oldRemaining * oldRemaining * oldRemaining;
    const int oldVisibleWidth = static_cast<int>(
        static_cast<float>(layout.panel.width) * oldBottomReveal + 0.5F);
    const int oldBottomBorder =
        layout.panel.x + layout.panel.width - oldVisibleWidth;
    CHECK_FALSE(opening24FpsFirst.pixel(
        oldBottomBorder, layout.panel.y + layout.panel.height - 1));

    std::size_t earlyMaskPixels = 0;
    std::size_t middleMaskPixels = 0;
    for (int y = 0; y < openingFirst.height(); ++y) {
      for (int x = 0; x < layout.panel.x; ++x) {
        if (openingEarly.pixel(x, y)) ++earlyMaskPixels;
        if (openingFirst.pixel(x, y)) ++middleMaskPixels;
      }
    }
    CHECK(earlyMaskPixels > 0);
    CHECK(middleMaskPixels > earlyMaskPixels);

    constexpr float kProgress = 0.35F;
    constexpr float kOpenBottomDelay = 0.22F;
    const float bottomProgress = std::max(
        0.0F, std::min(1.0F,
            (kProgress - kOpenBottomDelay) /
                (1.0F - kOpenBottomDelay)));
    const float remaining = 1.0F - bottomProgress;
    const float eased = 1.0F - remaining * remaining * remaining;
    std::size_t maskedUncoveredPixels = 0;
    const int sampleTop =
        layout.panel.y + layout.panel.height * 3 / 4;
    for (int y = sampleTop;
         y < layout.panel.y + layout.panel.height; ++y) {
      const float row = static_cast<float>(y - layout.panel.y) /
                        static_cast<float>(layout.panel.height - 1);
      const float rowReveal = 1.0F - (1.0F - eased) * row;
      const int visibleWidth = static_cast<int>(
          static_cast<float>(layout.panel.width) * rowReveal + 0.5F);
      const int destinationLeft =
          layout.panel.x + layout.panel.width - visibleWidth;
      for (int x = layout.panel.x; x < destinationLeft; ++x) {
        // This range is in the right half but precedes this scanline's panel
        // destination. Any black pixel therefore comes from the full-screen
        // mask, not from panel content.
        if (openingFirst.pixel(x, y)) ++maskedUncoveredPixels;
      }
    }
    CHECK(maskedUncoveredPixels > 0);
  }
}

TEST_CASE("volume indicator keeps every bar visible and hollows inactive bars") {
  using cadenza::audio::SoundVolume;
  const SoundVolume volumes[] = {SoundVolume::Muted, SoundVolume::Low,
                                 SoundVolume::Medium, SoundVolume::High};
  const std::int32_t activeCounts[] = {0, 1, 3, 5};
  constexpr cadenza::Rect kBounds{4, 4, 30, 14};
  for (std::size_t state = 0; state < 4; ++state) {
    cadenza::MonoFramebuffer frame{cadenza::FramebufferProfile::TEmbed};
    cadenza::MonoCanvas canvas{frame};
    cadenza::presentation::SystemUi::volumeIndicator(
        canvas, kBounds, volumes[state], false);
    const std::int32_t baseY = kBounds.y + kBounds.height - 3;
    for (std::int32_t bar = 0; bar < 5; ++bar) {
      const std::int32_t height = 3 + bar * 2;
      const std::int32_t x = kBounds.x + bar * 6;
      const std::int32_t top = baseY - height;
      CHECK(frame.pixel(x, top));
      CHECK(frame.pixel(x + 1, top + 1) == (bar < activeCounts[state]));
    }
  }

  cadenza::MonoFramebuffer inverted{cadenza::FramebufferProfile::TEmbed};
  inverted.clear(true);
  cadenza::MonoCanvas invertedCanvas{inverted};
  cadenza::presentation::SystemUi::volumeIndicator(
      invertedCanvas, kBounds, SoundVolume::Medium, true);
  CHECK_FALSE(inverted.pixel(kBounds.x, kBounds.y + 8));
}

TEST_CASE("Motion switch has distinct left and right knob positions") {
  constexpr cadenza::Rect kBounds{4, 4, 30, 14};
  cadenza::MonoFramebuffer disabled{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoFramebuffer enabled{cadenza::FramebufferProfile::TEmbed};
  cadenza::MonoCanvas disabledCanvas{disabled};
  cadenza::MonoCanvas enabledCanvas{enabled};
  cadenza::presentation::SystemUi::toggle(disabledCanvas, kBounds, false,
                                          false);
  cadenza::presentation::SystemUi::toggle(enabledCanvas, kBounds, true,
                                          false);

  CHECK(disabled.pixel(10, 9));
  CHECK_FALSE(disabled.pixel(24, 9));
  CHECK_FALSE(enabled.pixel(10, 9));
  CHECK(enabled.pixel(24, 9));

  cadenza::MonoFramebuffer inverted{cadenza::FramebufferProfile::TEmbed};
  inverted.clear(true);
  cadenza::MonoCanvas invertedCanvas{inverted};
  cadenza::presentation::SystemUi::toggle(invertedCanvas, kBounds, true,
                                          true);
  CHECK_FALSE(inverted.pixel(24, 9));
}

TEST_CASE("floating status indicator isolates itself from hostile backgrounds") {
  const cadenza::FramebufferProfile profiles[] = {
      cadenza::FramebufferProfile::TEmbed,
      cadenza::FramebufferProfile::Sharp};
  constexpr cadenza::Rect kBounds{8, 8, 78, 28};
  for (const auto profile : profiles) {
    for (int background = 0; background < 3; ++background) {
      CAPTURE(static_cast<int>(profile));
      CAPTURE(background);
      cadenza::MonoFramebuffer frame{profile};
      cadenza::MonoCanvas canvas{frame};
      frame.clear(background == 0);
      if (background == 2) {
        frame.clear(false);
        for (int y = 0; y < frame.height(); y += 2) {
          canvas.line(0, y, frame.width() - 1, y, true);
        }
      }
      const bool outsideBefore = frame.pixel(0, 0);

      cadenza::presentation::SystemUi::statusIndicator(
          canvas, kBounds, "T 10", true);

      const int right = kBounds.x + kBounds.width - 1;
      const int bottom = kBounds.y + kBounds.height - 1;
      for (int x = kBounds.x + 5; x <= right - 5; ++x) {
        CHECK_FALSE(frame.pixel(x, kBounds.y + 1));
        CHECK_FALSE(frame.pixel(x, bottom - 1));
      }
      for (int y = kBounds.y + 5; y <= bottom - 5; ++y) {
        CHECK_FALSE(frame.pixel(kBounds.x + 1, y));
        CHECK_FALSE(frame.pixel(right - 1, y));
      }
      CHECK(frame.pixel(kBounds.x + kBounds.width / 2, kBounds.y + 2));
      CHECK(frame.pixel(right - 5, kBounds.y + kBounds.height / 2));
      CHECK(frame.pixel(0, 0) == outsideBefore);
    }
  }
}

TEST_CASE("passive transient and status indicator have dual-profile goldens") {
  const cadenza::FramebufferProfile profiles[] = {
      cadenza::FramebufferProfile::TEmbed,
      cadenza::FramebufferProfile::Sharp};
  const std::uint64_t goldenHashes[] = {13413943807203622308ULL,
                                        7387506746915168240ULL};
  for (std::size_t profileIndex = 0; profileIndex < 2; ++profileIndex) {
    cadenza::MonoFramebuffer frame{profiles[profileIndex]};
    cadenza::MonoCanvas canvas{frame};
    cadenza::presentation::SystemSurfaceCoordinator surfaces;
    REQUIRE(surfaces.pushTransient("SAVED", 1.0F));
    cadenza::presentation::renderTransientFeedback(canvas, surfaces);
    cadenza::presentation::SystemUi::statusIndicator(
        canvas, {frame.width() - 82, 4, 78, 26}, "SYNC", true);
    CHECK(framebufferHash(frame) == goldenHashes[profileIndex]);
    CHECK_FALSE(surfaces.update(0.01F, {}, false).consumeInput);
  }
}
