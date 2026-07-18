#include "cadenza/apps/apps.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

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
}  // namespace

void LauncherApp::onEnter() noexcept {
  position_ = static_cast<float>(selected_);
  pulse_ = 1.0F;
}

void LauncherApp::update(const AppUpdateContext& context) noexcept {
  const Seconds dt = context.dt;
  const InputFrame& input = context.input;
  time_ += dt;
  pulse_ *= std::pow(0.015F, dt);
  const int appCount = context.catalog.launcherAppCount();
  if (appCount == 0) return;
  if (input.turn != 0) {
    const int previous = selected_;
    selected_ = wrap(selected_ + input.turn, appCount);
    pulse_ = 1.0F;
    context.commands.submit(SystemCommand::playSound(
        selected_ == previous ? audio::SoundCue::Boundary
                              : audio::SoundCue::Navigate));
    context.commands.submit(SystemCommand::emitDiagnostic(
        {DiagnosticCategory::Runtime, DiagnosticCode::SelectionChanged,
         "launcher selection", selected_}));
  }
  const float delta = static_cast<float>(selected_) - position_;
  position_ += delta * (1.0F - std::pow(0.0005F, dt));
  if (input.clicked) {
    context.navigator.open(context.catalog.launcherAppAt(selected_));
  }
}

void LauncherApp::render(MonoCanvas& canvas,
                         const AppRenderContext& context) noexcept {
  canvas.clear(false);
  const int width = canvas.width();
  const int height = canvas.height();
  const int centerX = width / 2;
  const int drift = static_cast<int>(time_ * 28.0F) % 20;
  for (int x = -height + drift; x < width; x += 20) {
    canvas.line(x, 0, x + height, height, true);
  }
  canvas.fillRect(0, 0, width, 28, true);
  canvas.text("CADENZA", 10, 14, 2, false, TextAlign::MiddleLeft);
  canvas.text("TURN  SELECT  PRESS  OPEN", width - 12, 14, 1, false,
              TextAlign::MiddleRight);

  const int cardX = width * 6 / 100;
  const int cardY = height * 25 / 100;
  const int cardWidth = width - cardX * 2;
  const int cardHeight = height * 54 / 100;
  canvas.fillRect(cardX, cardY, cardWidth, cardHeight, false);
  canvas.rect(cardX, cardY, cardWidth, cardHeight, true);
  canvas.rect(cardX + 3, cardY + 3, cardWidth - 6, cardHeight - 6, true);
  const int kick = static_cast<int>(pulse_ * 7.0F);
  canvas.fillRect(cardX + 13 - kick, cardY + 14,
                  cardWidth - 26 + kick * 2, cardHeight - 29, true);
  const int appCount = context.catalog.launcherAppCount();
  if (appCount == 0) {
    const Rect titleBounds{cardX + 17, cardY + 18, cardWidth - 34,
                           cardHeight - 37};
    canvas.boundedText(menuLabel("NO APPS", titleBounds, 4, 1,
                                 TextAlign::MiddleCenter),
                       false);
    return;
  }
  // The title is constrained to the smallest black card interior (kick == 0),
  // with four pixels of text padding on every side. The animated kick may
  // expand the background but never changes the text contract.
  const Rect titleBounds{cardX + 17, cardY + 18, cardWidth - 34,
                         cardHeight - 37};
  canvas.boundedText(
      menuLabel(context.catalog.appName(context.catalog.launcherAppAt(selected_)),
                titleBounds, 4, 1, TextAlign::MiddleCenter),
      false);

  const int previous = wrap(selected_ - 1, appCount);
  const int next = wrap(selected_ + 1, appCount);
  const int footerY = height - 17;
  // Reserve a stable 64-pixel navigation slot. Dynamic neighbor names are
  // isolated in the remaining left and right regions and cannot reach dots.
  constexpr int kFooterMargin = 12;
  constexpr int kNavigationWidth = 64;
  constexpr int kFooterLabelHeight = 13;
  const int navigationLeft = centerX - kNavigationWidth / 2;
  const Rect previousBounds{kFooterMargin,
                            footerY - kFooterLabelHeight / 2,
                            navigationLeft - kFooterMargin,
                            kFooterLabelHeight};
  const Rect nextBounds{navigationLeft + kNavigationWidth,
                        footerY - kFooterLabelHeight / 2,
                        width - kFooterMargin -
                            (navigationLeft + kNavigationWidth),
                        kFooterLabelHeight};
  canvas.boundedText(
      menuLabel(context.catalog.appName(context.catalog.launcherAppAt(previous)),
                previousBounds, 1, 1, TextAlign::MiddleLeft));
  canvas.boundedText(menuLabel(context.catalog.appName(
                                   context.catalog.launcherAppAt(next)),
                               nextBounds, 1, 1,
                               TextAlign::MiddleRight));
  const int dotsLeft = centerX - ((appCount - 1) * 9) / 2;
  for (int index = 0; index < appCount; ++index) {
    if (index == selected_) {
      canvas.fillCircle(dotsLeft + index * 9, footerY, 3, true);
    } else {
      canvas.circle(dotsLeft + index * 9, footerY, 2, true);
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
  canvas.clear(false);
  const int width = canvas.width();
  const int height = canvas.height();
  const int seconds = static_cast<int>(elapsed_);
  char value[16];
  std::snprintf(value, sizeof(value), "%02d:%02d", (seconds / 60) % 100,
                seconds % 60);
  const int sweep = static_cast<int>(std::fmod(elapsed_, 1.0F) * width);
  canvas.fillRect(0, 0, sweep, 9, true);
  canvas.text("CHRONO 01", 12, 24, 2, true);
  canvas.text(value, width / 2, height / 2, 4, true,
              TextAlign::MiddleCenter);
  for (int x = -10; x < width + 10; x += 16) {
    const int wobble =
        static_cast<int>(std::sin(phase_ * 5.0F + x * 0.08F) * 5.0F);
    canvas.fillCircle(x, height - 35 + wobble, 3, true);
  }
  canvas.text(running_ ? "PRESS TO PAUSE" : "PRESS TO RUN", 12,
              height - 12, 1, true, TextAlign::BottomLeft);
  canvas.text("HOLD: HOME", width - 12, height - 12, 1, true,
              TextAlign::BottomRight);
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
  canvas.clear(false);
  const int width = canvas.width();
  const int height = canvas.height();
  const int ballX = static_cast<int>(x_ * width);
  const int targetX = static_cast<int>(target_ * width);
  const int centerY = height / 2;
  for (int x = 0; x < width; x += 16) canvas.line(x, 30, x, height - 25);
  for (int y = 30; y <= height - 25; y += 16) canvas.line(0, y, width, y);
  canvas.fillRect(0, 0, width, 27, true);
  canvas.text("MOTION STUDY", 10, 14, 2, false, TextAlign::MiddleLeft);
  canvas.fillCircle(ballX, centerY, 28, false);
  canvas.circle(ballX, centerY, 28, true);
  canvas.fillCircle(ballX, centerY, 19, true);
  canvas.fillCircle(targetX, centerY, 4, true);
  const int tail = velocity_ > 0 ? -1 : 1;
  for (int index = 1; index <= 4; ++index) {
    canvas.line(ballX + tail * (30 + index * 8), centerY - 18 + index * 7,
                ballX + tail * (40 + index * 11),
                centerY - 18 + index * 7, true);
  }
  canvas.fillRect(0, height - 22, width, 22, false);
  canvas.text("TURN / PRESS TO THROW", 9, height - 11, 1, true,
              TextAlign::MiddleLeft);
  canvas.text("HOLD: HOME", width - 9, height - 11, 1, true,
              TextAlign::MiddleRight);
}

void SettingsApp::update(const AppUpdateContext& context) noexcept {
  const Seconds dt = context.dt;
  const InputFrame& input = context.input;
  time_ += dt;
  if (input.turn) {
    selected_ = wrap(selected_ + input.turn, 5);
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
    context.commands.submit(SystemCommand::playSound(audio::SoundCue::Reject));
  }
}

void SettingsApp::render(MonoCanvas& canvas,
                         const AppRenderContext& context) noexcept {
  canvas.clear(false);
  const int width = canvas.width();
  const int height = canvas.height();
  const bool energetic = context.system.motionProfile == MotionProfile::Normal;
  const int bar = energetic
                      ? static_cast<int>((std::sin(time_ * 8.0F) + 1.0F) * 35.0F)
                      : 18;
  canvas.fillRect(0, 0, width * 28 / 100 + bar, height, true);
  canvas.text("SET", 12, 18, 4, false);
  canvas.text("TINGS", 12, 50, 4, false);
  char soundRow[24];
  std::snprintf(soundRow, sizeof(soundRow), "SOUND: %s",
                audio::soundVolumeName(context.system.soundVolume));
  char wifiRow[24];
  const auto& wifi = context.system.connectivity.wifi;
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
  const auto provisioning = context.system.connectivity.provisioning.state;
  const char* provisioningState =
      resetConfirmArmed_
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
  const char* rows[5] = {energetic ? "MOTION: FULL" : "MOTION: QUIET",
                         soundRow, wifiRow, provisioningRow,
                         "ABOUT: CADENZA OS"};
  constexpr int rowHeight = 24;
  constexpr int rowStep = 27;
  const int rowsHeight = rowHeight + rowStep * 4;
  const int rowsTop = std::max(28, (height - rowsHeight) / 2);
  const int rowsLeft = width * 43 / 100;
  const int rowsWidth = width * 53 / 100;
  for (int index = 0; index < 5; ++index) {
    const int y = rowsTop + index * rowStep;
    if (index == selected_) {
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

}  // namespace cadenza
