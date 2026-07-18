#include "apps.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace {
int wrap(int value, int count) {
  value %= count;
  return value < 0 ? value + count : value;
}
}

void LauncherApp::onEnter() {
  position_ = static_cast<float>(selected_);
  pulse_ = 1.0f;
}

void LauncherApp::update(float dt, const InputFrame& input, AppRuntime& runtime) {
  time_ += dt;
  pulse_ *= std::pow(0.015f, dt);
  const int appCount = runtime.launcherAppCount();
  if (appCount == 0) return;
  if (input.turn != 0) {
    selected_ = wrap(selected_ + input.turn, appCount);
    pulse_ = 1.0f;
    runtime.emitDiagnostic({cadenza::DiagnosticCategory::Runtime,
                            cadenza::DiagnosticCode::SelectionChanged,
                            "launcher selection", selected_});
  }
  const float delta = static_cast<float>(selected_) - position_;
  position_ += delta * (1.0f - std::pow(0.0005f, dt));
  if (input.clicked) runtime.open(runtime.launcherAppAt(selected_));
}

void LauncherApp::render(MonoCanvas& c, const AppRuntime& runtime) {
  c.clear(false);
  const int width = c.width();
  const int height = c.height();
  const int centerX = width / 2;
  const int drift = static_cast<int>(time_ * 28.0f) % 20;
  for (int x = -height + drift; x < width; x += 20) c.line(x, 0, x + height, height, true);

  c.fillRect(0, 0, width, 28, true);
  c.text("CADENZA", 10, 14, 2, false, TextAlign::MiddleLeft);
  c.text("TURN  SELECT  PRESS  OPEN", width - 12, 14, 1, false, TextAlign::MiddleRight);

  const int cardX = width * 6 / 100;
  const int cardY = height * 25 / 100;
  const int cardW = width - cardX * 2;
  const int cardH = height * 54 / 100;
  c.fillRect(cardX, cardY, cardW, cardH, false);
  c.rect(cardX, cardY, cardW, cardH, true);
  c.rect(cardX + 3, cardY + 3, cardW - 6, cardH - 6, true);
  const int kick = static_cast<int>(pulse_ * 7.0f);
  c.fillRect(cardX + 13 - kick, cardY + 14, cardW - 26 + kick * 2, cardH - 29, true);
  const int appCount = runtime.launcherAppCount();
  if (appCount == 0) {
    c.text("NO APPS", centerX, cardY + cardH / 2, 4, false, TextAlign::MiddleCenter);
    return;
  }
  c.text(runtime.appName(runtime.launcherAppAt(selected_)), centerX, cardY + cardH / 2, 4, false,
         TextAlign::MiddleCenter);

  const int previous = wrap(selected_ - 1, appCount);
  const int next = wrap(selected_ + 1, appCount);
  const int footerY = height - 17;
  c.text(runtime.appName(runtime.launcherAppAt(previous)), 12, footerY, 1, true,
         TextAlign::MiddleLeft);
  c.text(runtime.appName(runtime.launcherAppAt(next)), width - 12, footerY, 1, true,
         TextAlign::MiddleRight);
  const int dotsLeft = centerX - ((appCount - 1) * 9) / 2;
  for (int i = 0; i < appCount; ++i) {
    if (i == selected_) c.fillCircle(dotsLeft + i * 9, footerY, 3, true);
    else c.circle(dotsLeft + i * 9, footerY, 2, true);
  }
}

void ClockApp::onEnter() { phase_ = 0.0f; }

void ClockApp::update(float dt, const InputFrame& input, AppRuntime&) {
  phase_ += dt;
  if (running_) elapsed_ += dt;
  if (input.clicked) running_ = !running_;
  if (input.turn) elapsed_ = std::max(0.0f, elapsed_ + input.turn * 10.0f);
}

void ClockApp::render(MonoCanvas& c, const AppRuntime&) {
  c.clear(false);
  const int width = c.width();
  const int height = c.height();
  const int seconds = static_cast<int>(elapsed_);
  char value[16];
  std::snprintf(value, sizeof(value), "%02d:%02d", (seconds / 60) % 100, seconds % 60);
  const int sweep = static_cast<int>(std::fmod(elapsed_, 1.0f) * width);
  c.fillRect(0, 0, sweep, 9, true);
  c.text("CHRONO 01", 12, 24, 2, true, TextAlign::TopLeft);
  c.text(value, width / 2, height / 2, 4, true, TextAlign::MiddleCenter);
  for (int x = -10; x < width + 10; x += 16) {
    const int wobble = static_cast<int>(std::sin(phase_ * 5.0f + x * 0.08f) * 5.0f);
    c.fillCircle(x, height - 35 + wobble, 3, true);
  }
  c.text(running_ ? "PRESS TO PAUSE" : "PRESS TO RUN", 12, height - 12, 1, true,
         TextAlign::BottomLeft);
  c.text("HOLD: HOME", width - 12, height - 12, 1, true, TextAlign::BottomRight);
}

void MotionApp::onEnter() { velocity_ = 0.0f; }

void MotionApp::update(float dt, const InputFrame& input, AppRuntime&) {
  time_ += dt;
  if (input.turn) target_ += input.turn * 0.106f;
  if (input.clicked) target_ = target_ < 0.5f ? 0.84f : 0.16f;
  target_ = std::max(0.11f, std::min(0.89f, target_));
  const float acceleration = (target_ - x_) * 75.0f - velocity_ * 12.0f;
  velocity_ += acceleration * dt;
  x_ += velocity_ * dt;
}

void MotionApp::render(MonoCanvas& c, const AppRuntime&) {
  c.clear(false);
  const int width = c.width();
  const int height = c.height();
  const int ballX = static_cast<int>(x_ * width);
  const int targetX = static_cast<int>(target_ * width);
  const int centerY = height / 2;
  for (int x = 0; x < width; x += 16) c.line(x, 30, x, height - 25, true);
  for (int y = 30; y <= height - 25; y += 16) c.line(0, y, width, y, true);
  c.fillRect(0, 0, width, 27, true);
  c.text("MOTION STUDY", 10, 14, 2, false, TextAlign::MiddleLeft);
  c.fillCircle(ballX, centerY, 28, false);
  c.circle(ballX, centerY, 28, true);
  c.fillCircle(ballX, centerY, 19, true);
  c.fillCircle(targetX, centerY, 4, true);
  const int tail = velocity_ > 0 ? -1 : 1;
  for (int i = 1; i <= 4; ++i) c.line(ballX + tail * (30 + i * 8), centerY - 18 + i * 7,
                                      ballX + tail * (40 + i * 11), centerY - 18 + i * 7, true);
  c.fillRect(0, height - 22, width, 22, false);
  c.text("TURN / PRESS TO THROW", 9, height - 11, 1, true, TextAlign::MiddleLeft);
  c.text("HOLD: HOME", width - 9, height - 11, 1, true, TextAlign::MiddleRight);
}

void SettingsApp::update(float dt, const InputFrame& input, AppRuntime&) {
  time_ += dt;
  if (input.turn) selected_ = wrap(selected_ + input.turn, 2);
  if (input.clicked && selected_ == 0) energetic_ = !energetic_;
}

void SettingsApp::render(MonoCanvas& c, const AppRuntime&) {
  c.clear(false);
  const int width = c.width();
  const int height = c.height();
  const int bar = energetic_ ? static_cast<int>((std::sin(time_ * 8.0f) + 1.0f) * 35.0f) : 18;
  c.fillRect(0, 0, width * 28 / 100 + bar, height, true);
  c.text("SET", 12, 18, 4, false, TextAlign::TopLeft);
  c.text("TINGS", 12, 50, 4, false, TextAlign::TopLeft);
  const char* rows[2] = {energetic_ ? "MOTION: FULL" : "MOTION: QUIET", "CADENZA OS"};
  for (int i = 0; i < 2; ++i) {
    const int y = 53 + i * 48;
    if (i == selected_) {
      c.fillRect(width * 43 / 100, y, width * 53 / 100, 34, true);
      c.text(rows[i], width * 46 / 100, y + 17, 2, false, TextAlign::MiddleLeft);
    } else {
      c.rect(width * 43 / 100, y, width * 53 / 100, 34, true);
      c.text(rows[i], width * 46 / 100, y + 17, 2, true, TextAlign::MiddleLeft);
    }
  }
  c.text("HOLD TO GO HOME", width - 12, height - 12, 1, true, TextAlign::BottomRight);
}
