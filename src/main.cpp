#include <Arduino.h>
#include <TFT_eSPI.h>

#include <algorithm>

#include "board_pins.h"
#include "cadenza/core/app_runtime.h"
#include "cadenza/core/apps.h"
#include "cadenza/core/mono_canvas.h"
#include "cadenza/core/mono_framebuffer.h"
#include "input.h"
#include "i2s_audio_output.h"
#include "serial_diagnostic_sink.h"
#include "tft_presenter.h"

namespace {
TFT_eSPI display;
cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
cadenza::MonoCanvas canvas{framebuffer};
TftPresenter presenter(display);
InputController input;
cadenza::AppRuntime runtime{cadenza::FramebufferProfile::TEmbed};
SerialDiagnosticSink diagnosticSink;
I2sAudioOutput audioOutput;
cadenza::LauncherApp launcher;
cadenza::ClockApp clockApp;
cadenza::MotionApp motion;
cadenza::SettingsApp settings;
cadenza::AnimationGalleryApp gallery;
uint32_t lastFrameUs = 0;

struct LcdCommand {
  uint8_t command;
  uint8_t data[14];
  uint8_t length;
};

constexpr LcdCommand kLcdCommands[] = {
    {0x11, {0}, 0x80}, {0x3A, {0x05}, 1}, {0xB2, {0x0B, 0x0B, 0x00, 0x33, 0x33}, 5},
    {0xB7, {0x75}, 1}, {0xBB, {0x28}, 1}, {0xC0, {0x2C}, 1}, {0xC2, {0x01}, 1},
    {0xC3, {0x1F}, 1}, {0xC6, {0x13}, 1}, {0xD0, {0xA7}, 1},
    {0xD0, {0xA4, 0xA1}, 2}, {0xD6, {0xA1}, 1},
    {0xE0, {0xF0, 0x05, 0x0A, 0x06, 0x06, 0x03, 0x2B, 0x32, 0x43, 0x36, 0x11, 0x10, 0x2B, 0x32}, 14},
    {0xE1, {0xF0, 0x08, 0x0C, 0x0B, 0x09, 0x24, 0x2B, 0x22, 0x43, 0x38, 0x15, 0x16, 0x2F, 0x37}, 14},
};

void applyPanelInitialization() {
  for (const auto& entry : kLcdCommands) {
    display.writecommand(entry.command);
    for (uint8_t i = 0; i < (entry.length & 0x7F); ++i) display.writedata(entry.data[i]);
    if (entry.length & 0x80) delay(120);
  }
}
}

void setup() {
  pinMode(BoardPins::kPowerOn, OUTPUT);
  digitalWrite(BoardPins::kPowerOn, HIGH);
  Serial.begin(115200);
  delay(150);
  Serial.println("Cadenza OS runtime starting");
  Serial.printf("Flash: %u MB, PSRAM: %u MB\n", ESP.getFlashChipSize() / 1048576U,
                ESP.getPsramSize() / 1048576U);

  display.begin();
  applyPanelInitialization();
  display.setRotation(3);
  pinMode(BoardPins::kLcdBacklight, OUTPUT);
  digitalWrite(BoardPins::kLcdBacklight, HIGH);

  Serial.printf("Display: T-Embed ST7789, %dx%d\n", framebuffer.width(),
                framebuffer.height());
  input.begin();
  runtime.setDiagnosticSink(&diagnosticSink);
  runtime.registerApp(cadenza::AppId::Launcher, launcher, false);
  runtime.registerApp(cadenza::AppId::Clock, clockApp);
  runtime.registerApp(cadenza::AppId::Motion, motion);
  runtime.registerApp(cadenza::AppId::Settings, settings);
  runtime.registerApp(cadenza::AppId::Gallery, gallery);
  runtime.begin(cadenza::AppId::Launcher);
  if (!audioOutput.begin(runtime, &diagnosticSink)) {
    Serial.println("Audio disabled; graphics runtime remains active");
  }
  lastFrameUs = micros();
}

void loop() {
  input.sample();
  const uint32_t now = micros();
  if (now - lastFrameUs < 16667U) {
    delay(1);
    return;
  }
  const float dt = std::min((now - lastFrameUs) / 1000000.0f, 0.05f);
  lastFrameUs = now;
  runtime.update(dt, input.takeFrame());
  runtime.render(canvas);
  presenter.present(framebuffer);
}
