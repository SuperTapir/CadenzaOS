#include "cadenza/apps/apps.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "generated/clock_cover.h"
#include "generated/clock_t_embed_cover.h"
#include "generated/gallery_cover.h"
#include "generated/gallery_t_embed_cover.h"
#include "generated/motion_cover.h"
#include "generated/motion_t_embed_cover.h"
#include "generated/settings_cover.h"
#include "generated/settings_t_embed_cover.h"

namespace cadenza {
namespace {
int wrap(int value, int count) noexcept {
  value %= count;
  return value < 0 ? value + count : value;
}

BoundedTextRequest menuLabel(const char* value, Rect bounds,
                             std::uint8_t preferredScale,
                             std::uint8_t minimumScale,
                             TextAlign align) noexcept {
  BoundedTextRequest request;
  request.value = value;
  request.bounds = bounds;
  request.preferredScale = preferredScale;
  request.minimumScale = minimumScale;
  request.align = align;
  request.overflow = TextOverflowPolicy::Ellipsis;
  request.maximumLines = 1;
  return request;
}

Rect intersectRect(Rect a, Rect b) noexcept {
  const int left = std::max(a.x, b.x);
  const int top = std::max(a.y, b.y);
  const int right = std::min(a.x + a.width, b.x + b.width);
  const int bottom = std::min(a.y + a.height, b.y + b.height);
  return {left, top, std::max(0, right - left),
          std::max(0, bottom - top)};
}

bool hasArea(Rect rect) noexcept { return rect.width > 0 && rect.height > 0; }

void fillWithin(MonoCanvas& canvas, Rect rect, Rect viewport,
                bool black) noexcept {
  const Rect visible = intersectRect(rect, viewport);
  if (hasArea(visible)) {
    canvas.fillRect(visible.x, visible.y, visible.width, visible.height,
                    black);
  }
}

void borderWithin(MonoCanvas& canvas, Rect rect, Rect viewport,
                  bool black) noexcept {
  if (rect.width <= 0 || rect.height <= 0) return;
  const int left = rect.x;
  const int right = rect.x + rect.width - 1;
  const int top = rect.y;
  const int bottom = rect.y + rect.height - 1;
  const int viewportRight = viewport.x + viewport.width - 1;
  const int viewportBottom = viewport.y + viewport.height - 1;
  const int horizontalLeft = std::max(left, viewport.x);
  const int horizontalRight = std::min(right, viewportRight);
  const int verticalTop = std::max(top, viewport.y);
  const int verticalBottom = std::min(bottom, viewportBottom);
  if (horizontalLeft <= horizontalRight && top >= viewport.y &&
      top <= viewportBottom) {
    canvas.line(horizontalLeft, top, horizontalRight, top, black);
  }
  if (horizontalLeft <= horizontalRight && bottom >= viewport.y &&
      bottom <= viewportBottom && bottom != top) {
    canvas.line(horizontalLeft, bottom, horizontalRight, bottom, black);
  }
  if (verticalTop <= verticalBottom && left >= viewport.x &&
      left <= viewportRight) {
    canvas.line(left, verticalTop, left, verticalBottom, black);
  }
  if (verticalTop <= verticalBottom && right >= viewport.x &&
      right <= viewportRight && right != left) {
    canvas.line(right, verticalTop, right, verticalBottom, black);
  }
}

Rect inset(Rect rect, int amount) noexcept {
  if (amount <= 0) return rect;
  return {rect.x + amount, rect.y + amount,
          std::max(0, rect.width - amount * 2),
          std::max(0, rect.height - amount * 2)};
}

void renderFallbackCover(MonoCanvas& canvas, Rect bounds,
                         const char* title) noexcept {
  if (!hasArea(bounds)) return;
  canvas.fillRect(bounds.x, bounds.y, bounds.width, bounds.height, false);
  if (bounds.width < 34 || bounds.height < 20) return;
  const int iconSize = std::min(24, std::max(8, bounds.height / 4));
  const int iconX = bounds.x + std::max(7, bounds.width / 14);
  const int iconY = bounds.y + (bounds.height - iconSize) / 2;
  canvas.rect(iconX, iconY, iconSize, iconSize, true);
  const Rect textBounds{iconX + iconSize + 8, bounds.y + 5,
                        bounds.x + bounds.width - (iconX + iconSize + 13),
                        bounds.height - 10};
  if (textBounds.width >= 24 && textBounds.height >= 10) {
    canvas.boundedText(
        menuLabel(title, textBounds, 3, 1, TextAlign::MiddleLeft), true);
  }
}

bool renderBitmapCover(MonoCanvas& canvas, Rect bounds,
                       const BitmapView& tEmbedBitmap,
                       const BitmapView& sharpBitmap,
                       bool blackBackground) noexcept {
  if (!hasArea(bounds)) return false;
  const BitmapView& bitmap = bounds.width <= tEmbedBitmap.width
                                 ? tEmbedBitmap
                                 : sharpBitmap;
  if (!bitmap.valid()) return false;
  canvas.fillRect(bounds.x, bounds.y, bounds.width, bounds.height,
                  blackBackground);
  const int copyWidth = std::min(bounds.width, bitmap.width);
  const int copyHeight = std::min(bounds.height, bitmap.height);
  if (copyWidth <= 0 || copyHeight <= 0) return true;
  const Rect source{(bitmap.width - copyWidth) / 2,
                    (bitmap.height - copyHeight) / 2,
                    copyWidth, copyHeight};
  const int destinationX = bounds.x + (bounds.width - copyWidth) / 2;
  const int destinationY = bounds.y + (bounds.height - copyHeight) / 2;
  canvas.drawBitmap(bitmap, source, destinationX, destinationY);
  return true;
}

float launchProgress(float progress) noexcept {
  return std::max(0.0F, std::min(1.0F, progress));
}

Rect centeredLaunchCover(const MonoCanvas& canvas) noexcept {
  const int width = std::min(350, canvas.width() * 7 / 8);
  const int height = width * 155 / 350;
  return {(canvas.width() - width) / 2, (canvas.height() - height) / 2,
          width, height};
}

std::uint8_t ditherCoverage(float progress) noexcept {
  return static_cast<std::uint8_t>(
      std::max(0.0F, std::min(64.0F, progress * 64.0F)));
}

float smoothStep(float progress) noexcept {
  const float p = launchProgress(progress);
  return p * p * (3.0F - 2.0F * p);
}

void blendCenteredCoverIntoTarget(
    MonoCanvas& canvas, float progress, const BitmapView& tEmbedBitmap,
    const BitmapView& sharpBitmap, bool blackBackground) noexcept {
  constexpr float kCoverHold = 0.22F;
  const float blend = progress <= kCoverHold
                          ? 0.0F
                          : smoothStep((progress - kCoverHold) /
                                       (1.0F - kCoverHold));
  const std::uint8_t coverage = ditherCoverage(blend);
  if (coverage >= 64) return;

  const Rect bounds = centeredLaunchCover(canvas);
  const BitmapView& bitmap = bounds.width <= tEmbedBitmap.width
                                 ? tEmbedBitmap
                                 : sharpBitmap;
  const int copyWidth = std::min(bounds.width, bitmap.width);
  const int copyHeight = std::min(bounds.height, bitmap.height);
  const Rect source{(bitmap.width - copyWidth) / 2,
                    (bitmap.height - copyHeight) / 2,
                    copyWidth, copyHeight};
  const int bitmapLeft = bounds.x + (bounds.width - copyWidth) / 2;
  const int bitmapTop = bounds.y + (bounds.height - copyHeight) / 2;
  for (int y = 0; y < canvas.height(); ++y) {
    for (int x = 0; x < canvas.width(); ++x) {
      if (kOrderedDither8x8.thresholds[(y & 7) * 8 + (x & 7)] <
          coverage) {
        continue;
      }
      bool coverPixel = true;
      if (x >= bounds.x && x < bounds.x + bounds.width &&
          y >= bounds.y && y < bounds.y + bounds.height) {
        coverPixel = blackBackground;
        if (x >= bitmapLeft && x < bitmapLeft + copyWidth &&
            y >= bitmapTop && y < bitmapTop + copyHeight) {
          coverPixel = bitmap.pixel(source.x + x - bitmapLeft,
                                    source.y + y - bitmapTop);
        }
      }
      canvas.pixel(x, y, coverPixel);
    }
  }
}

void renderClockScreen(MonoCanvas& canvas, float elapsed, float phase,
                       bool running) noexcept {
  canvas.clear(false);
  const int width = canvas.width();
  const int height = canvas.height();
  const int seconds = static_cast<int>(elapsed);
  char value[16];
  std::snprintf(value, sizeof(value), "%02d:%02d", (seconds / 60) % 100,
                seconds % 60);
  const int sweep = static_cast<int>(std::fmod(elapsed, 1.0F) * width);
  canvas.fillRect(0, 0, sweep, 9, true);
  canvas.text("CHRONO 01", 12, 24, 2, true);
  canvas.text(value, width / 2, height / 2, 4, true,
              TextAlign::MiddleCenter);
  for (int x = -10; x < width + 10; x += 16) {
    const int wobble =
        static_cast<int>(std::sin(phase * 5.0F + x * 0.08F) * 5.0F);
    canvas.fillCircle(x, height - 35 + wobble, 3, true);
  }
  canvas.text(running ? "PRESS TO PAUSE" : "PRESS TO RUN", 12,
              height - 12, 1, true, TextAlign::BottomLeft);
  canvas.text("HOLD: HOME", width - 12, height - 12, 1, true,
              TextAlign::BottomRight);
}

void renderMotionScreen(MonoCanvas& canvas, float position, float target,
                        float velocity) noexcept {
  canvas.clear(false);
  const int width = canvas.width();
  const int height = canvas.height();
  const int ballX = static_cast<int>(position * width);
  const int targetX = static_cast<int>(target * width);
  const int centerY = height / 2;
  for (int x = 0; x < width; x += 16) canvas.line(x, 30, x, height - 25);
  for (int y = 30; y <= height - 25; y += 16) canvas.line(0, y, width, y);
  canvas.fillRect(0, 0, width, 27, true);
  canvas.text("MOTION STUDY", 10, 14, 2, false, TextAlign::MiddleLeft);
  canvas.fillCircle(ballX, centerY, 28, false);
  canvas.circle(ballX, centerY, 28, true);
  canvas.fillCircle(ballX, centerY, 19, true);
  canvas.fillCircle(targetX, centerY, 4, true);
  const int tail = velocity > 0 ? -1 : 1;
  for (int index = 1; index <= 4; ++index) {
    canvas.line(ballX + tail * (30 + index * 8),
                centerY - 18 + index * 7,
                ballX + tail * (40 + index * 11),
                centerY - 18 + index * 7, true);
  }
  canvas.fillRect(0, height - 22, width, 22, false);
  canvas.text("TURN / PRESS TO THROW", 9, height - 11, 1, true,
              TextAlign::MiddleLeft);
  canvas.text("HOLD: HOME", width - 9, height - 11, 1, true,
              TextAlign::MiddleRight);
}

void renderSettingsScreen(MonoCanvas& canvas, const SystemSnapshot& system,
                          int selected, float time,
                          bool resetConfirmArmed) noexcept {
  canvas.clear(false);
  const int width = canvas.width();
  const int height = canvas.height();
  const bool energetic = system.motionProfile == MotionProfile::Normal;
  const int bar = energetic
                      ? static_cast<int>((std::sin(time * 8.0F) + 1.0F) *
                                         35.0F)
                      : 18;
  canvas.fillRect(0, 0, width * 28 / 100 + bar, height, true);
  canvas.text("SET", 12, 18, 4, false);
  canvas.text("TINGS", 12, 50, 4, false);
  char soundRow[24];
  std::snprintf(soundRow, sizeof(soundRow), "SOUND: %s",
                audio::soundVolumeName(system.soundVolume));
  char wifiRow[24];
  const auto& wifi = system.connectivity.wifi;
  const char* wifiState = wifi.networkOnline()
                              ? "ONLINE"
                              : wifi.needsUserAction()
                                    ? "ACTION"
                                    : wifi.desired ==
                                              WiFiDesiredPolicy::OnlineRequested
                                          ? "CONNECT"
                                          : "OFF";
  std::snprintf(wifiRow, sizeof(wifiRow), "WIFI: %s", wifiState);
  char provisioningRow[28];
  const auto provisioning = system.connectivity.provisioning.state;
  const char* provisioningState =
      resetConfirmArmed
          ? "CONFIRM RESET"
          : provisioning == ProvisioningState::Advertising
                ? "ADVERTISING"
                : provisioning == ProvisioningState::Negotiating
                      ? "PAIRING"
                      : provisioning == ProvisioningState::Applying ||
                                provisioning == ProvisioningState::Verifying
                            ? "APPLYING"
                            : provisioning == ProvisioningState::Stopping
                                  ? "STOPPING"
                                  : provisioning == ProvisioningState::Failed
                                        ? "FAILED"
                                        : "START";
  std::snprintf(provisioningRow, sizeof(provisioningRow), "SETUP: %s",
                provisioningState);
  const char* launcherRow =
      system.launcherOrientation == LauncherOrientation::Vertical
          ? "LAUNCHER: VERTICAL"
          : "LAUNCHER: HORIZONTAL";
  const char* rows[6] = {energetic ? "MOTION: FULL" : "MOTION: QUIET",
                         soundRow, wifiRow, provisioningRow, launcherRow,
                         "ABOUT: CADENZA OS"};
  const int rowHeight = height < 200 ? 20 : 24;
  const int rowStep = height < 200 ? 22 : 27;
  const int rowsHeight = rowHeight + rowStep * 5;
  const int rowsTop = std::max(24, (height - rowsHeight) / 2);
  const int rowsLeft = width * 43 / 100;
  const int rowsWidth = width * 53 / 100;
  for (int index = 0; index < 6; ++index) {
    const int y = rowsTop + index * rowStep;
    if (index == selected) {
      canvas.fillRect(rowsLeft, y, rowsWidth, rowHeight, true);
      canvas.boundedText(
          menuLabel(rows[index], {rowsLeft + 7, y + 2, rowsWidth - 12,
                                  rowHeight - 4},
                    2, 1, TextAlign::MiddleLeft),
          false);
    } else {
      canvas.rect(rowsLeft, y, rowsWidth, rowHeight, true);
      canvas.boundedText(
          menuLabel(rows[index], {rowsLeft + 7, y + 2, rowsWidth - 12,
                                  rowHeight - 4},
                    2, 1, TextAlign::MiddleLeft),
          true);
    }
  }
  canvas.text("HOLD: HOME", 12, height - 8, 1, false,
              TextAlign::BottomLeft);
}

void renderLauncherCard(MonoCanvas& canvas, Rect card, Rect viewport,
                        AppId appId, const char* title,
                        const AppCatalogView& catalog) noexcept {
  const Rect visible = intersectRect(card, viewport);
  if (!hasArea(visible)) return;
  fillWithin(canvas, card, viewport, false);
  borderWithin(canvas, card, viewport, true);
  const Rect content = inset(card, 3);
  const Rect visibleContent = intersectRect(content, viewport);
  if (!hasArea(visibleContent)) return;
  const Rect previousClip = canvas.clip();
  const bool previousClipDiagnostics = canvas.reportsGeometryClips();
  if (!canvas.setClip(visibleContent, false)) return;
  if (!catalog.renderLauncherCover(appId, canvas, content)) {
    renderFallbackCover(canvas, content, title);
  }
  canvas.setClip(previousClip, previousClipDiagnostics);
}
}  // namespace

void LauncherApp::onEnter() noexcept {
  position_ = static_cast<float>(targetPosition_);
  trackSpring_.reset(position_);
  settled_ = true;
}

void LauncherApp::update(const AppUpdateContext& context) noexcept {
  const Seconds dt = context.dt;
  const InputFrame& input = context.input;
  const int appCount = context.catalog.launcherAppCount();
  if (appCount == 0) {
    selected_ = 0;
    targetPosition_ = 0;
    position_ = 0.0F;
    trackSpring_.reset(0.0F);
    settled_ = true;
    return;
  }
  if (motionProfile_ != context.system.motionProfile) {
    motionProfile_ = context.system.motionProfile;
    trackSpring_.reset(position_);
    trackSpring_.setTarget(static_cast<float>(targetPosition_));
  }
  if (input.turn != 0) {
    const int previous = selected_;
    targetPosition_ += static_cast<std::int64_t>(input.turn);
    selected_ = wrap(static_cast<int>(targetPosition_ % appCount), appCount);
    settled_ = false;
    context.commands.submit(SystemCommand::playSound(
        selected_ == previous ? audio::SoundCue::Boundary
                              : audio::SoundCue::Navigate));
    context.commands.submit(SystemCommand::emitDiagnostic(
        {DiagnosticCategory::Runtime, DiagnosticCode::SelectionChanged,
         "launcher selection", selected_}));
  }
  const float target = static_cast<float>(targetPosition_);
  if (motionProfile_ == MotionProfile::Normal) {
    trackSpring_.setTarget(target);
    trackSpring_.update(dt);
    position_ = trackSpring_.value();
    settled_ = trackSpring_.settled();
  } else {
    const float delta = target - position_;
    if (std::abs(delta) <= 0.0005F) {
      position_ = target;
      settled_ = true;
    } else {
      const float next =
          position_ + delta * (1.0F - std::pow(0.0005F, dt));
      if ((dt > 0.0F && next == position_) ||
          std::abs(target - next) <= 0.0005F) {
        position_ = target;
        settled_ = true;
      } else {
        position_ = next;
        settled_ = false;
      }
    }
    trackSpring_.reset(position_);
    trackSpring_.setTarget(target);
  }
  constexpr std::int64_t kRebaseThreshold = 4096;
  if (settled_ && std::abs(targetPosition_) >= kRebaseThreshold) {
    const std::int64_t canonical = selected_;
    const std::int64_t shift = targetPosition_ - canonical;
    targetPosition_ = canonical;
    position_ -= static_cast<float>(shift);
    trackSpring_.reset(position_);
    trackSpring_.setTarget(static_cast<float>(targetPosition_));
  }
  if (input.clicked) {
    context.navigator.open(context.catalog.launcherAppAt(selected_));
  }
}

void LauncherApp::render(MonoCanvas& canvas,
                         const AppRenderContext& context) noexcept {
  canvas.clear(false);
  const int width = canvas.width();
  const int height = canvas.height();
  // Maximum-density pure scanlines for a 1-bit framebuffer: one dark row,
  // one light row. The phase is screen-anchored and never crawls with cards.
  for (int y = 1; y < height; y += 2) {
    canvas.line(0, y, width - 1, y, true);
  }
  const int centerX = width / 2;
  const Rect viewport{0, 0, width, height};
  const int appCount = context.catalog.launcherAppCount();
  if (appCount == 0) {
    const int cardWidth = width * 88 / 100;
    const int cardHeight = viewport.height * 72 / 100;
    renderLauncherCard(
        canvas,
        {centerX - cardWidth / 2,
         viewport.y + (viewport.height - cardHeight) / 2, cardWidth,
         cardHeight},
        viewport, {}, "NO APPS", context.catalog);
  } else {
    const bool vertical = context.system.launcherOrientation ==
                          LauncherOrientation::Vertical;
    // Cover art owns a fixed 350:155 canvas. The Launcher sizes its card
    // around a complete profile-specific image instead of cropping artwork
    // differently for horizontal and vertical navigation.
    const int contentWidth = std::min(350, width * 7 / 8);
    const int contentHeight = contentWidth * 155 / 350;
    const int cardWidth = contentWidth + 6;
    const int cardHeight = contentHeight + 6;
    const int pitch = vertical ? cardHeight + std::max(8, height / 20)
                               : cardWidth + std::max(10, width / 30);
    const int centerY = viewport.y + viewport.height / 2;
    const std::int64_t base =
        static_cast<std::int64_t>(std::floor(position_));
    for (std::int64_t slot = base - 3; slot <= base + 3; ++slot) {
      const float offset = static_cast<float>(slot) - position_;
      const int cardCenterX =
          vertical ? centerX
                   : centerX + static_cast<int>(std::round(offset * pitch));
      const int cardCenterY =
          vertical ? centerY + static_cast<int>(std::round(offset * pitch))
                   : centerY;
      const Rect card{cardCenterX - cardWidth / 2,
                      cardCenterY - cardHeight / 2, cardWidth, cardHeight};
      const int appIndex = wrap(static_cast<int>(slot % appCount), appCount);
      const AppId appId = context.catalog.launcherAppAt(appIndex);
      renderLauncherCard(
          canvas, card, viewport, appId, context.catalog.appName(appId),
          context.catalog);
    }
  }
}

void ClockApp::onEnter() noexcept { phase_ = 0.0F; }

void ClockApp::update(const AppUpdateContext& context) noexcept {
  const Seconds dt = context.dt;
  const InputFrame& input = context.input;
  phase_ += dt;
  if (running_) elapsed_ += dt;
  if (input.clicked) {
    running_ = !running_;
    context.commands.submit(SystemCommand::playSound(
        running_ ? audio::SoundCue::ToggleOn : audio::SoundCue::ToggleOff));
  }
  if (input.turn) {
    const float previous = elapsed_;
    elapsed_ = std::max(0.0F, elapsed_ + input.turn * 10.0F);
    context.commands.submit(SystemCommand::playSound(
        elapsed_ == previous ? audio::SoundCue::Boundary
                             : audio::SoundCue::Navigate));
  }
}

void ClockApp::render(MonoCanvas& canvas, const AppRenderContext&) noexcept {
  renderClockScreen(canvas, elapsed_, phase_, running_);
}

bool ClockApp::renderLauncherCover(MonoCanvas& canvas,
                                   Rect bounds) const noexcept {
  return renderBitmapCover(canvas, bounds, kClockTEmbedCover,
                           kClockCover, true);
}

bool ClockApp::renderLaunchFrame(MonoCanvas& canvas,
                                 float progress,
                                 const AppRenderContext&) const noexcept {
  const float p = launchProgress(progress);
  renderClockScreen(canvas, elapsed_, 0.0F, running_);
  blendCenteredCoverIntoTarget(canvas, p, kClockTEmbedCover, kClockCover,
                               true);
  return true;
}

void MotionApp::onEnter() noexcept { velocity_ = 0.0F; }

void MotionApp::update(const AppUpdateContext& context) noexcept {
  const Seconds dt = context.dt;
  const InputFrame& input = context.input;
  time_ += dt;
  const float previousTarget = target_;
  if (input.turn) target_ += input.turn * 0.106F;
  if (input.clicked) target_ = target_ < 0.5F ? 0.84F : 0.16F;
  target_ = std::max(0.11F, std::min(0.89F, target_));
  if (input.turn) {
    context.commands.submit(SystemCommand::playSound(
        target_ == previousTarget ? audio::SoundCue::Boundary
                                  : audio::SoundCue::Navigate));
  } else if (input.clicked) {
    context.commands.submit(SystemCommand::playSound(
        target_ >= 0.5F ? audio::SoundCue::ToggleOn
                        : audio::SoundCue::ToggleOff));
  }
  const float acceleration = (target_ - x_) * 75.0F - velocity_ * 12.0F;
  velocity_ += acceleration * dt;
  x_ += velocity_ * dt;
}

void MotionApp::render(MonoCanvas& canvas, const AppRenderContext&) noexcept {
  renderMotionScreen(canvas, x_, target_, velocity_);
}

bool MotionApp::renderLauncherCover(MonoCanvas& canvas,
                                    Rect bounds) const noexcept {
  return renderBitmapCover(canvas, bounds, kMotionTEmbedCover,
                           kMotionCover, false);
}

bool MotionApp::renderLaunchFrame(MonoCanvas& canvas,
                                  float progress,
                                  const AppRenderContext&) const noexcept {
  const float p = launchProgress(progress);
  renderMotionScreen(canvas, x_, target_, 0.0F);
  blendCenteredCoverIntoTarget(canvas, p, kMotionTEmbedCover, kMotionCover,
                               false);
  return true;
}

void SettingsApp::update(const AppUpdateContext& context) noexcept {
  const Seconds dt = context.dt;
  const InputFrame& input = context.input;
  time_ += dt;
  if (input.turn) {
    selected_ = wrap(selected_ + input.turn, 6);
    resetConfirmArmed_ = false;
    context.commands.submit(
        SystemCommand::playSound(audio::SoundCue::Navigate));
  }
  if (input.clicked && selected_ == 0) {
    const bool becomingReduced =
        context.system.motionProfile == MotionProfile::Normal;
    context.commands.submit(SystemCommand::setMotionProfile(
        becomingReduced ? MotionProfile::Reduced : MotionProfile::Normal));
    context.commands.submit(SystemCommand::playSound(
        becomingReduced ? audio::SoundCue::ToggleOff
                        : audio::SoundCue::ToggleOn));
  } else if (input.clicked && selected_ == 1) {
    const audio::SoundVolume next =
        audio::nextSoundVolume(context.system.soundVolume);
    if (context.commands.submit(SystemCommand::setSoundVolume(next)) &&
        next != audio::SoundVolume::Muted) {
      context.commands.submit(
          SystemCommand::playSound(audio::SoundCue::ToggleOn));
    }
  } else if (input.clicked && selected_ == 2) {
    const bool requested = context.system.connectivity.wifi.desired !=
                           WiFiDesiredPolicy::OnlineRequested;
    context.commands.submit(
        SystemCommand::setNetworkOnlineRequested(requested));
    context.commands.submit(SystemCommand::playSound(
        requested ? audio::SoundCue::ToggleOn : audio::SoundCue::ToggleOff));
  } else if (input.clicked && selected_ == 3) {
    const ProvisioningState state =
        context.system.connectivity.provisioning.state;
    const bool active = state == ProvisioningState::Advertising ||
                        state == ProvisioningState::Negotiating ||
                        state == ProvisioningState::Applying ||
                        state == ProvisioningState::Verifying ||
                        state == ProvisioningState::Stopping;
    if (active) {
      context.commands.submit(SystemCommand::cancelProvisioning());
      resetConfirmArmed_ = false;
    } else {
      const bool resetRequired =
          state == ProvisioningState::Failed ||
          context.system.connectivity.wifi.needsUserAction();
      if (resetRequired && !resetConfirmArmed_) {
        resetConfirmArmed_ = true;
        context.commands.submit(
            SystemCommand::playSound(audio::SoundCue::Boundary));
      } else {
        context.commands.submit(
            SystemCommand::startProvisioning(resetRequired));
        resetConfirmArmed_ = false;
      }
    }
  } else if (input.clicked && selected_ == 4) {
    const bool becomingHorizontal =
        context.system.launcherOrientation == LauncherOrientation::Vertical;
    context.commands.submit(SystemCommand::setLauncherOrientation(
        becomingHorizontal ? LauncherOrientation::Horizontal
                           : LauncherOrientation::Vertical));
    context.commands.submit(SystemCommand::playSound(
        becomingHorizontal ? audio::SoundCue::ToggleOn
                           : audio::SoundCue::ToggleOff));
  } else if (input.clicked && selected_ == 5) {
    context.commands.submit(SystemCommand::playSound(audio::SoundCue::Reject));
  }
}

void SettingsApp::render(MonoCanvas& canvas,
                         const AppRenderContext& context) noexcept {
  renderSettingsScreen(canvas, context.system, selected_, time_,
                       resetConfirmArmed_);
}

bool SettingsApp::renderLauncherCover(MonoCanvas& canvas,
                                      Rect bounds) const noexcept {
  return renderBitmapCover(canvas, bounds, kSettingsTEmbedCover,
                           kSettingsCover, false);
}

bool SettingsApp::renderLaunchFrame(MonoCanvas& canvas,
                                    float progress,
                                    const AppRenderContext& context) const noexcept {
  const float p = launchProgress(progress);
  renderSettingsScreen(canvas, context.system, selected_, time_,
                       resetConfirmArmed_);
  blendCenteredCoverIntoTarget(canvas, p, kSettingsTEmbedCover,
                               kSettingsCover, false);
  return true;
}

bool AnimationGalleryApp::renderLauncherCover(
    MonoCanvas& canvas, Rect bounds) const noexcept {
  return renderBitmapCover(canvas, bounds, kGalleryTEmbedCover,
                           kGalleryCover, true);
}

bool AnimationGalleryApp::renderLaunchFrame(
    MonoCanvas& canvas, float progress,
    const AppRenderContext&) const noexcept {
  const float p = launchProgress(progress);
  renderInitialFrame(canvas);
  blendCenteredCoverIntoTarget(canvas, p, kGalleryTEmbedCover, kGalleryCover,
                               true);
  return true;
}

}  // namespace cadenza
