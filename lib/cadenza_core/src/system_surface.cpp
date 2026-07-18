#include "cadenza/presentation/system_surface.h"

#include <algorithm>
#include <cstring>

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
    bool captureFrame) noexcept {
  interactive_ = SurfaceKind::SystemMenu;
  selection_ = SystemMenuItem::Resume;
  menuArmed_ = !captureUntilRelease_;
  revealProgress_ = 0.0F;
  ++diagnostics_.opened;
  return {true, captureFrame, SystemSurfaceIntent::Opened};
}

SystemSurfaceFrame SystemSurfaceCoordinator::closeMenu() noexcept {
  interactive_ = SurfaceKind::None;
  menuArmed_ = false;
  revealProgress_ = 0.0F;
  ++diagnostics_.closed;
  return {true, false, SystemSurfaceIntent::Closed};
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
    bool homeCurrent) noexcept {
  expireTransients(dt);
  if (menuActive()) {
    constexpr float kRevealDuration = 0.16F;
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
    return openMenu(true);
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
    return openMenu(true);
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
      interactive_ = SurfaceKind::None;
      menuArmed_ = false;
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
  openMenu(true);
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
  request.preferredScale = 2;
  request.minimumScale = 1;
  request.align = TextAlign::MiddleLeft;
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
  request.preferredScale = 2;
  request.minimumScale = 1;
  request.align = TextAlign::MiddleLeft;
  canvas.boundedText(request, ink);
  value(canvas, {bounds.x + bounds.width * 2 / 3, bounds.y,
                 bounds.width / 3 - 5, bounds.height}, valueText, selected);
}

void SystemUi::selection(MonoCanvas& canvas, Rect bounds,
                         bool selected) noexcept {
  if (selected) {
    canvas.fillRect(bounds.x + 2, bounds.y + 1, bounds.width - 4,
                    bounds.height - 2, true);
  }
}

void SystemUi::value(MonoCanvas& canvas, Rect bounds, const char* text,
                     bool inverted) noexcept {
  if (!text || !*text) return;
  BoundedTextRequest request;
  request.value = text;
  request.bounds = bounds;
  request.align = TextAlign::MiddleRight;
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
  canvas.rect(bounds.x, bounds.y, bounds.width, bounds.height, true);
  if (active) canvas.fillRect(bounds.x + 2, bounds.y + 2, 5,
                              bounds.height - 4, true);
  canvas.text(label, bounds.x + 10, bounds.y + bounds.height / 2, 1, true,
              TextAlign::MiddleLeft);
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

void renderTransientFeedback(
    MonoCanvas& canvas, const SystemSurfaceCoordinator& surfaces) noexcept {
  if (surfaces.transientCount() == 0) return;
  const TransientFeedback* feedback =
      surfaces.transientAt(surfaces.transientCount() - 1);
  if (!feedback) return;
  const Rect bounds{10, canvas.height() - 29, canvas.width() - 20, 20};
  canvas.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, false);
  canvas.rect(bounds.x, bounds.y, bounds.width, bounds.height, true);
  canvas.text(feedback->text.data(), bounds.x + bounds.width / 2,
              bounds.y + bounds.height / 2, 1, true,
              TextAlign::MiddleCenter);
}

}  // namespace cadenza::presentation
