#include "cadenza/presentation/system_surface.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

#include "cadenza/animation/easing.h"

namespace cadenza::presentation {
namespace {

constexpr std::size_t menuIndex(SystemMenuItem item) noexcept {
  return static_cast<std::size_t>(item);
}

constexpr std::size_t visibleMenuCount(bool homeCurrent) noexcept {
  return homeCurrent ? 3U : 4U;
}

constexpr SystemMenuItem visibleMenuItem(bool homeCurrent,
                                         std::size_t position) noexcept {
  if (!homeCurrent) return static_cast<SystemMenuItem>(position);
  return position == 0 ? SystemMenuItem::Resume
                       : static_cast<SystemMenuItem>(position + 1U);
}

std::size_t visibleMenuPosition(bool homeCurrent,
                                SystemMenuItem item) noexcept {
  const std::size_t count = visibleMenuCount(homeCurrent);
  for (std::size_t position = 0; position < count; ++position) {
    if (visibleMenuItem(homeCurrent, position) == item) return position;
  }
  return 0;
}

float outCubic(float progress) noexcept {
  const float t = std::max(0.0F, std::min(1.0F, progress));
  const float remaining = 1.0F - t;
  return 1.0F - remaining * remaining * remaining;
}

void overlayDitherMask(MonoCanvas& canvas, Rect bounds,
                       std::uint8_t coverage) noexcept {
  coverage = std::min<std::uint8_t>(coverage, 64);
  const Rect clip = canvas.clip();
  const std::int32_t left = std::max(bounds.x, clip.x);
  const std::int32_t top = std::max(bounds.y, clip.y);
  const std::int32_t right = std::min(bounds.x + bounds.width,
                                      clip.x + clip.width);
  const std::int32_t bottom = std::min(bounds.y + bounds.height,
                                       clip.y + clip.height);
  for (std::int32_t y = top; y < bottom; ++y) {
    for (std::int32_t x = left; x < right; ++x) {
      const std::size_t patternIndex =
          static_cast<std::size_t>(y & 7) * 8U +
          static_cast<std::size_t>(x & 7);
      if (kOrderedDither8x8.thresholds[patternIndex] < coverage) {
        canvas.pixel(x, y, true);
      }
    }
  }
}

}  // namespace

void SystemSurfaceCoordinator::reject(SurfaceRejection reason) noexcept {
  diagnostics_.lastRejection = reason;
  ++diagnostics_.rejected;
  if (diagnosticSink_) {
    diagnosticSink_->emit(
        {reason == SurfaceRejection::TransientQueueFull ||
                 reason == SurfaceRejection::Busy
             ? DiagnosticCategory::Capacity
             : DiagnosticCategory::Runtime,
         reason == SurfaceRejection::TransientQueueFull ||
                 reason == SurfaceRejection::Busy
             ? DiagnosticCode::CapacityExceeded
             : DiagnosticCode::InvalidOperation,
         "system surface rejected", static_cast<std::int32_t>(reason)});
  }
}

SystemSurfaceFrame SystemSurfaceCoordinator::openMenu(
    bool captureFrame, MotionProfile motionProfile) noexcept {
  interactive_ = SurfaceKind::SystemMenu;
  selection_ = SystemMenuItem::Resume;
  menuArmed_ = !captureUntilRelease_;
  menuClosing_ = false;
  revealProgress_ = 0.0F;
  menuMotionProfile_ = motionProfile;
  ++diagnostics_.opened;
  return {true, captureFrame, SystemSurfaceIntent::Opened};
}

SystemSurfaceFrame SystemSurfaceCoordinator::closeMenu() noexcept {
  menuArmed_ = false;
  menuClosing_ = true;
  ++diagnostics_.closed;
  return {true, false, SystemSurfaceIntent::Closed};
}

void SystemSurfaceCoordinator::releaseMenu() noexcept {
  interactive_ = SurfaceKind::None;
  menuArmed_ = false;
  menuClosing_ = false;
  revealProgress_ = 0.0F;
}

void SystemSurfaceCoordinator::expireTransients(Seconds dt) noexcept {
  const float elapsed = std::max(0.0F, dt);
  std::size_t output = 0;
  for (std::size_t index = 0; index < transientCount_; ++index) {
    transients_[index].remainingSeconds -= elapsed;
    if (transients_[index].remainingSeconds > 0.0F) {
      if (output != index) transients_[output] = transients_[index];
      ++output;
    }
  }
  transientCount_ = output;
}

SystemSurfaceFrame SystemSurfaceCoordinator::update(
    Seconds dt, const InputFrame& input, bool appTransitioning,
    bool homeCurrent, MotionProfile motionProfile,
    TimerSnapshot timer) noexcept {
  expireTransients(dt);
  if (input.pressed) buttonSequenceActive_ = true;

  if (timer.state == TimerState::Expired && !timerAlertActive()) {
    interactive_ = SurfaceKind::TimerAlert;
    menuArmed_ = false;
    menuClosing_ = false;
    deferredMenu_ = false;
    openWhenStable_ = false;
    captureUntilRelease_ = false;
    revealProgress_ = 1.0F;
    timerAlertElapsed_ = 0.0F;
    timerAlertArmed_ =
        !buttonSequenceActive_ && !input.pressed && !input.released &&
        !input.clicked && !input.longPressed && input.heldMs == 0;
    ++diagnostics_.opened;
    return {true, true, SystemSurfaceIntent::None};
  }

  if (timerAlertActive()) {
    if (timer.state != TimerState::Expired) {
      interactive_ = SurfaceKind::None;
      timerAlertArmed_ = false;
      timerAlertElapsed_ = 0.0F;
      if (input.released) buttonSequenceActive_ = false;
      ++diagnostics_.closed;
      return {true, false, SystemSurfaceIntent::None};
    }
    timerAlertElapsed_ += std::max(0.0F, dt);
    if (!timerAlertArmed_) {
      if (input.released) {
        buttonSequenceActive_ = false;
        timerAlertArmed_ = true;
      }
      return {true, false, SystemSurfaceIntent::None};
    }
    if (input.clicked) {
      if (input.released) buttonSequenceActive_ = false;
      timerAlertElapsed_ = 0.0F;
      return {true, false, SystemSurfaceIntent::TimerAcknowledge};
    }
    if (input.released) buttonSequenceActive_ = false;
    return {true, false, SystemSurfaceIntent::None};
  }

  if (input.released) buttonSequenceActive_ = false;
  if (menuClosing_) {
    if (captureUntilRelease_ && input.released) {
      captureUntilRelease_ = false;
    }
    const float duration =
        menuMotionProfile_ == MotionProfile::Reduced ? 0.10F : 0.20F;
    revealProgress_ = std::max(
        0.0F, revealProgress_ - std::max(0.0F, dt) / duration);
    if (revealProgress_ <= 0.0F) releaseMenu();
    return {true, false, SystemSurfaceIntent::None};
  }
  if (menuActive()) {
    const float kRevealDuration =
        menuMotionProfile_ == MotionProfile::Reduced ? 0.10F : 0.20F;
    revealProgress_ = std::min(
        1.0F, revealProgress_ + std::max(0.0F, dt) / kRevealDuration);
  }

  if (input.longPressed) {
    captureUntilRelease_ = true;
    menuArmed_ = false;
    if (appTransitioning) {
      deferredMenu_ = true;
      return {true, false, SystemSurfaceIntent::None};
    }
    if (menuActive()) return closeMenu();
    return openMenu(true, motionProfile);
  }

  if (captureUntilRelease_) {
    if (input.released) {
      captureUntilRelease_ = false;
      menuArmed_ = menuActive();
    }
    return {true, false, SystemSurfaceIntent::None};
  }

  if (openWhenStable_ && !appTransitioning) {
    openWhenStable_ = false;
    deferredMenu_ = false;
    return openMenu(true, motionProfile);
  }

  if (!menuActive()) return {};

  SystemSurfaceFrame frame{true, false, SystemSurfaceIntent::None};
  if (input.turn != 0) {
    const int count = static_cast<int>(visibleMenuCount(homeCurrent));
    int next = static_cast<int>(visibleMenuPosition(homeCurrent, selection_)) +
               static_cast<int>(input.turn);
    next = std::max(0, std::min(count - 1, next));
    const SystemMenuItem nextItem =
        visibleMenuItem(homeCurrent, static_cast<std::size_t>(next));
    if (nextItem == selection_) {
      frame.intent = SystemSurfaceIntent::Boundary;
    } else {
      selection_ = nextItem;
      frame.intent = SystemSurfaceIntent::Navigate;
    }
    return frame;
  }
  if (!menuArmed_ || !input.clicked) return frame;

  switch (selection_) {
    case SystemMenuItem::Resume: return closeMenu();
    case SystemMenuItem::Home:
      if (homeCurrent) {
        frame.intent = SystemSurfaceIntent::Boundary;
        return frame;
      }
      releaseMenu();
      ++diagnostics_.closed;
      frame.intent = SystemSurfaceIntent::Home;
      return frame;
    case SystemMenuItem::Sound: frame.intent = SystemSurfaceIntent::Sound; break;
    case SystemMenuItem::Motion: frame.intent = SystemSurfaceIntent::Motion; break;
    case SystemMenuItem::Count:
      reject(SurfaceRejection::InvalidAction);
      frame.intent = SystemSurfaceIntent::Boundary;
      break;
  }
  return frame;
}

void SystemSurfaceCoordinator::notifyTransitionStable() noexcept {
  if (deferredMenu_) openWhenStable_ = true;
}

bool SystemSurfaceCoordinator::requestInteractive(SurfaceKind kind) noexcept {
  if (kind == SurfaceKind::None) {
    if (!menuActive()) return true;
    closeMenu();
    return true;
  }
  if (kind != SurfaceKind::SystemMenu) {
    reject(SurfaceRejection::DeniedOwner);
    return false;
  }
  if (interactive_ != SurfaceKind::None) {
    reject(SurfaceRejection::Busy);
    return false;
  }
  openMenu(true, MotionProfile::Normal);
  return true;
}

bool SystemSurfaceCoordinator::rejectInvalidAction() noexcept {
  reject(SurfaceRejection::InvalidAction);
  return false;
}

bool SystemSurfaceCoordinator::pushTransient(const char* text,
                                             Seconds duration) noexcept {
  if (!text || !*text || duration <= 0.0F) {
    reject(SurfaceRejection::InvalidAction);
    return false;
  }
  if (transientCount_ >= kTransientCapacity) {
    ++diagnostics_.transientDropped;
    reject(SurfaceRejection::TransientQueueFull);
    return false;
  }
  TransientFeedback& feedback = transients_[transientCount_++];
  std::strncpy(feedback.text.data(), text, feedback.text.size() - 1);
  feedback.text.back() = '\0';
  feedback.remainingSeconds = duration;
  diagnostics_.transientHighWater = std::max(
      diagnostics_.transientHighWater,
      static_cast<std::uint8_t>(transientCount_));
  return true;
}

const TransientFeedback* SystemSurfaceCoordinator::transientAt(
    std::size_t index) const noexcept {
  return index < transientCount_ ? &transients_[index] : nullptr;
}

SystemMenuLayout SystemMenuLayout::forCanvas(std::int32_t width,
                                             std::int32_t height) noexcept {
  const std::int32_t margin = 0;
  const std::int32_t panelWidth = std::min<std::int32_t>(
      width, width / 2);
  const std::int32_t panelHeight = height - margin * 2;
  const Rect panel{width - panelWidth - margin, margin, panelWidth,
                   panelHeight};
  const std::int32_t headerHeight = 0;
  const Rect inner{panel.x + 4, panel.y + 4, panel.width - 8,
                   panel.height - 8};
  return {panel,
          {panel.x + 2, panel.y, panel.width - 2, headerHeight},
          inner,
          std::min<std::int32_t>(
              30, inner.height /
                      static_cast<std::int32_t>(SystemMenuItem::Count))};
}

void SystemUi::panel(MonoCanvas& canvas, Rect bounds) noexcept {
  canvas.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, false);
  canvas.fillRect(bounds.x, bounds.y, 2, bounds.height, true);
}

void SystemUi::header(MonoCanvas& canvas, Rect bounds,
                      const char* title) noexcept {
  canvas.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, true);
  BoundedTextRequest request;
  request.value = title;
  request.bounds = {bounds.x + 6, bounds.y, bounds.width - 12, bounds.height};
  request.align = TextAlign::MiddleLeft;
  request.role = TextRole::Compact;
  canvas.boundedText(request, false);
}

void SystemUi::menuRow(MonoCanvas& canvas, Rect bounds, const char* label,
                       const char* valueText, bool selected,
                       bool disabled) noexcept {
  selection(canvas, bounds, selected);
  const bool ink = !selected;
  if (disabled) {
    canvas.fillDither(bounds, kOrderedDither8x8, 12, 0, 0, ink);
  }
  BoundedTextRequest request;
  request.value = label;
  request.bounds = {bounds.x + 6, bounds.y,
                    bounds.width * 2 / 3 - 6,
                    bounds.height};
  request.align = TextAlign::MiddleLeft;
  request.role = TextRole::Menu;
  canvas.boundedText(request, ink);
  value(canvas, {bounds.x + bounds.width * 2 / 3, bounds.y,
                 bounds.width / 3 - 5, bounds.height}, valueText, selected);
}

void SystemUi::selection(MonoCanvas& canvas, Rect bounds,
                         bool selected) noexcept {
  if (selected) {
    canvas.fillRoundedRect(bounds.x, bounds.y + 1, bounds.width,
                           bounds.height - 2, 4, true);
  }
}

void SystemUi::value(MonoCanvas& canvas, Rect bounds, const char* text,
                     bool inverted) noexcept {
  if (!text || !*text) return;
  BoundedTextRequest request;
  request.value = text;
  request.bounds = bounds;
  request.align = TextAlign::MiddleRight;
  request.role = TextRole::Caption;
  canvas.boundedText(request, !inverted);
}

void SystemUi::volumeIndicator(MonoCanvas& canvas, Rect bounds,
                               audio::SoundVolume volume,
                               bool inverted) noexcept {
  const bool ink = !inverted;
  std::int32_t active = 0;
  switch (volume) {
    case audio::SoundVolume::Muted: active = 0; break;
    case audio::SoundVolume::Low: active = 1; break;
    case audio::SoundVolume::Medium: active = 3; break;
    case audio::SoundVolume::High: active = 5; break;
    case audio::SoundVolume::Count: active = 0; break;
  }
  const std::int32_t baseY = bounds.y + bounds.height - 3;
  for (std::int32_t index = 0; index < 5; ++index) {
    const std::int32_t barHeight = 3 + index * 2;
    const std::int32_t x = bounds.x + index * 6;
    if (index < active) {
      canvas.fillRect(x, baseY - barHeight, 4, barHeight, ink);
    } else {
      canvas.rect(x, baseY - barHeight, 4, barHeight, ink);
    }
  }
}

void SystemUi::toggle(MonoCanvas& canvas, Rect bounds, bool enabled,
                      bool inverted) noexcept {
  const bool ink = !inverted;
  constexpr std::int32_t kSwitchWidth = 28;
  constexpr std::int32_t kSwitchHeight = 14;
  const std::int32_t x = bounds.x + bounds.width - kSwitchWidth;
  const std::int32_t y = bounds.y + (bounds.height - kSwitchHeight) / 2;
  canvas.rect(x, y, kSwitchWidth, kSwitchHeight, ink);
  constexpr std::int32_t kKnobSize = 8;
  const std::int32_t knobX = enabled ? x + kSwitchWidth - kKnobSize - 3
                                     : x + 3;
  canvas.fillRect(knobX, y + 3, kKnobSize, kKnobSize, ink);
}

void SystemUi::statusIndicator(MonoCanvas& canvas, Rect bounds,
                               const char* label, bool active) noexcept {
  constexpr std::int32_t kIsolation = 2;
  const Rect body{bounds.x + kIsolation, bounds.y + kIsolation,
                  bounds.width - kIsolation * 2,
                  bounds.height - kIsolation * 2};
  canvas.fillRoundedRect(bounds.x, bounds.y, bounds.width, bounds.height, 4,
                         false);
  canvas.fillRoundedRect(body.x, body.y, body.width, body.height, 3, true);

  const Rect stateRail{body.x + 3, body.y + 4, 3, body.height - 8};
  if (active) {
    canvas.fillRect(stateRail.x, stateRail.y, stateRail.width,
                    stateRail.height, false);
  } else {
    canvas.rect(stateRail.x, stateRail.y, stateRail.width, stateRail.height,
                false);
  }

  const TextMetrics labelMetrics =
      canvas.measureText(label, TextRole::Compact);
  const std::int32_t opticalOffset =
      labelMetrics.height > body.height - 4 ? 1 : 0;
  canvas.text(label, body.x + 10,
              body.y + body.height / 2 + opticalOffset, 1, false,
              TextAlign::MiddleLeft, TextRole::Compact);
}

void renderSystemMenu(MonoCanvas& canvas, SystemMenuItem selection,
                      bool homeCurrent,
                      const SystemSnapshot& snapshot,
                      float revealProgress) noexcept {
  static constexpr const char* kLabels[] = {
      "RESUME", "HOME", "SOUND", "MOTION"};
  SystemMenuLayout layout =
      SystemMenuLayout::forCanvas(canvas.width(), canvas.height());
  const float reveal = std::max(0.0F, std::min(1.0F, revealProgress));
  const std::int32_t hiddenOffset = canvas.width() - layout.panel.x;
  const std::int32_t offset = static_cast<std::int32_t>(
      static_cast<float>(hiddenOffset) * (1.0F - reveal) + 0.5F);
  layout.panel.x += offset;
  layout.header.x += offset;
  layout.rows.x += offset;
  overlayDitherMask(
      canvas, {0, 0, layout.panel.x, canvas.height()},
      static_cast<std::uint8_t>(8.0F * reveal + 0.5F));
  SystemUi::panel(canvas, layout.panel);
  const std::size_t itemCount = visibleMenuCount(homeCurrent);
  for (std::size_t position = 0; position < itemCount; ++position) {
    const auto item = visibleMenuItem(homeCurrent, position);
    const std::size_t index = menuIndex(item);
    const Rect row{layout.rows.x,
                   layout.rows.y + static_cast<std::int32_t>(position) *
                                       layout.rowHeight,
                   layout.rows.width, layout.rowHeight};
    const bool selected = item == selection;
    SystemUi::menuRow(canvas, row, kLabels[index], "", selected,
                      false);
    const Rect indicator{row.x + row.width - 36,
                         row.y + (row.height - 14) / 2, 30, 14};
    switch (item) {
      case SystemMenuItem::Sound:
        SystemUi::volumeIndicator(canvas, indicator, snapshot.soundVolume,
                                  selected);
        break;
      case SystemMenuItem::Motion:
        SystemUi::toggle(canvas, indicator,
                         snapshot.motionProfile == MotionProfile::Normal,
                         selected);
        break;
      case SystemMenuItem::Resume:
      case SystemMenuItem::Home:
      case SystemMenuItem::Count: break;
    }
  }
}

void renderSystemMenu(MonoCanvas& canvas, MonoFramebuffer& scratch,
                      SystemMenuItem selection, bool homeCurrent,
                      const SystemSnapshot& snapshot, float revealProgress,
                      bool closing) noexcept {
  scratch.clear(false);
  MonoCanvas scratchCanvas{scratch};
  renderSystemMenu(scratchCanvas, selection, homeCurrent, snapshot, 1.0F);

  const SystemMenuLayout layout =
      SystemMenuLayout::forCanvas(canvas.width(), canvas.height());
  const float raw = std::max(0.0F, std::min(1.0F, revealProgress));
  const float eased = outCubic(raw);
  overlayDitherMask(
      canvas, {0, 0, canvas.width(), canvas.height()},
      static_cast<std::uint8_t>(8.0F * eased + 0.5F));

  if (raw <= 0.0F) return;

  // The reference Menu is a directional sheet rather than a rigid drawer.
  // Opening pins the top edge to its settled position immediately while the
  // bottom waits through the first 24 FPS step, then catches up out-cubic.
  // Closing starts at the top again and lets the bottom trail for 40% of the
  // travel. Linear interpolation between those endpoints preserves the
  // reference's strong, straight diagonal leading edge; horizontal
  // resampling supplies the visible stretch/snap.
  constexpr float kOpenBottomDelay = 0.22F;
  constexpr float kCloseBottomDelay = 0.40F;
  const float closeProgress = 1.0F - raw;
  const float openingTopReveal = 1.0F;
  const float openingBottomProgress = std::max(
      0.0F, std::min(1.0F,
          (raw - kOpenBottomDelay) / (1.0F - kOpenBottomDelay)));
  const float openingBottomReveal = outCubic(openingBottomProgress);
  const float closingTopReveal = 1.0F - outCubic(closeProgress);
  const float closingBottomProgress = std::max(
      0.0F, std::min(1.0F,
          (closeProgress - kCloseBottomDelay) /
              (1.0F - kCloseBottomDelay)));
  const float closingBottomReveal =
      1.0F - outCubic(closingBottomProgress);

  const int panelRight = layout.panel.x + layout.panel.width;
  for (std::int32_t y = layout.panel.y;
       y < layout.panel.y + layout.panel.height; ++y) {
    const float row = layout.panel.height <= 1
                          ? 0.0F
                          : static_cast<float>(y - layout.panel.y) /
                                static_cast<float>(layout.panel.height - 1);
    const float topReveal =
        closing ? closingTopReveal : openingTopReveal;
    const float bottomReveal =
        closing ? closingBottomReveal : openingBottomReveal;
    const float rowReveal =
        topReveal + (bottomReveal - topReveal) * row;
    const int visibleWidth = static_cast<int>(
        static_cast<float>(layout.panel.width) * rowReveal + 0.5F);
    if (visibleWidth <= 0) continue;
    const int destinationLeft = panelRight - visibleWidth;
    for (int x = destinationLeft; x < panelRight; ++x) {
      const int sourceX = layout.panel.x +
          (x - destinationLeft) * layout.panel.width / visibleWidth;
      canvas.pixel(x, y, scratch.pixel(
          std::min<std::int32_t>(
              layout.panel.x + layout.panel.width - 1, sourceX), y));
    }
  }
}

void renderTransientFeedback(
    MonoCanvas& canvas, const SystemSurfaceCoordinator& surfaces) noexcept {
  if (surfaces.transientCount() == 0) return;
  const TransientFeedback* feedback =
      surfaces.transientAt(surfaces.transientCount() - 1);
  if (!feedback) return;
  const Rect bounds{10, canvas.height() - 35, canvas.width() - 20, 26};
  canvas.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, false);
  canvas.rect(bounds.x, bounds.y, bounds.width, bounds.height, true);
  canvas.text(feedback->text.data(), bounds.x + bounds.width / 2,
              bounds.y + bounds.height / 2, 1, true,
              TextAlign::MiddleCenter, TextRole::Compact);
}

void renderTimerAlert(MonoCanvas& canvas, TimerSnapshot timer,
                      float visualElapsed,
                      MotionProfile motionProfile) noexcept {
  const int width = canvas.width();
  const int height = canvas.height();
  canvas.fillRect(0, 0, width, height, true);
  const Rect card{12, 12, width - 24, height - 24};
  canvas.fillRect(card.x, card.y, card.width, card.height, false);
  canvas.rect(card.x, card.y, card.width, card.height, true);
  canvas.rect(card.x + 4, card.y + 4, card.width - 8,
              card.height - 8, true);
  if (motionProfile != MotionProfile::Reduced) {
    constexpr float kPeriodSeconds = 1.2F;
    const float phase =
        std::fmod(std::max(0.0F, visualElapsed), kPeriodSeconds) /
        kPeriodSeconds;
    const float pulse = 1.0F - std::abs(phase * 2.0F - 1.0F);
    const int railHeight = 10 + static_cast<int>(pulse * 22.0F + 0.5F);
    const int railTop = height / 2 - railHeight / 2;
    canvas.fillRect(card.x + 6, railTop, 2, railHeight, true);
    canvas.fillRect(card.x + card.width - 8, railTop, 2, railHeight, true);
  }

  canvas.text("TIME UP", width / 2, height / 2 - 28, 1, true,
              TextAlign::MiddleCenter, TextRole::Hero);
  char duration[32];
  const std::uint32_t configuredMinutes =
      (timer.configuredDurationMs +
       static_cast<std::uint32_t>(kTimerMinuteMs) - 1U) /
      static_cast<std::uint32_t>(kTimerMinuteMs);
  std::snprintf(duration, sizeof(duration), "%u MINUTES COMPLETE",
                static_cast<unsigned>(configuredMinutes));
  canvas.text(duration, width / 2, height / 2 + 18, 1,
              true, TextAlign::MiddleCenter, TextRole::Caption);
  constexpr const char* kAction = "PRESS TO CONTINUE";
  const TextMetrics actionMetrics =
      canvas.measureText(kAction, TextRole::Compact);
  const int actionWidth =
      std::min(card.width - 32, actionMetrics.width + 24);
  const int actionHeight = actionMetrics.height + 8;
  const int actionX = width / 2 - actionWidth / 2;
  const int actionY = height - 26 - actionHeight;
  canvas.fillRoundedRect(actionX, actionY, actionWidth, actionHeight, 4, true);
  canvas.text(kAction, width / 2, actionY + actionHeight / 2, 1, false,
              TextAlign::MiddleCenter, TextRole::Compact);
}

}  // namespace cadenza::presentation
