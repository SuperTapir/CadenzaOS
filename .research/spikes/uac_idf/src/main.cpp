/* Build-only spike: prove Arduino + portable Cadenza voice + UAC2/CDC. */
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Arduino.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tusb.h"
#include "usb_device_uac.h"
#if CONFIG_CADENZA_T_EMBED_RUNTIME
#include "cadenza/apps/apps.h"
#include "cadenza/core/app_runtime.h"
#include "cadenza/core/mono_canvas.h"
#include "cadenza/core/mono_framebuffer.h"
#include "cadenza/system/frame_coordinator.h"
#include "cadenza/system/system_service_host.h"
#if CONFIG_CADENZA_CONNECTIVITY_RUNTIME
#include "idf_connectivity_adapter.h"
#endif
#include "t_embed_display.h"
#include "t_embed_input.h"
#endif
#include "cadenza/voice/usb_voice_packetizer.h"
#include "cadenza/voice/usb_voice_session_bridge.h"
#include "t_embed_microphone.h"
#include "t_embed_speaker.h"
#include "uac_lifecycle_adapter.h"

namespace {

struct VoiceAdapter {
#if CONFIG_CADENZA_T_EMBED_RUNTIME
  explicit VoiceAdapter(cadenza::voice::VoiceCaptureCoordinator& coordinator)
      : capture(coordinator), packetizer(capture), sessionBridge(capture) {}
  cadenza::voice::VoiceCaptureCoordinator& capture;
#else
  cadenza::voice::VoiceCaptureCoordinator capture;
#endif
  cadenza::voice::UsbVoicePacketizer packetizer{capture};
  cadenza::voice::UsbVoiceSessionBridge sessionBridge{capture};
  std::uint64_t sequence = 0;
};

#if CONFIG_CADENZA_T_EMBED_RUNTIME
class LogDiagnosticSink final : public cadenza::DiagnosticSink {
 public:
  void emit(const cadenza::DiagnosticEvent& event) noexcept override {
    ESP_LOGI("cadenza_diag", "category=%u code=%u value=%ld %s",
             static_cast<unsigned>(event.category),
             static_cast<unsigned>(event.code), static_cast<long>(event.value),
             event.message ? event.message : "");
  }
};

cadenza::system::SystemServiceHost services;
#if CONFIG_CADENZA_CONNECTIVITY_RUNTIME
cadenza::idf::IdfConnectivityAdapter connectivity{services};
#endif
VoiceAdapter voice{services.voiceCapture()};
cadenza::MonoFramebuffer framebuffer{cadenza::FramebufferProfile::TEmbed};
cadenza::MonoCanvas canvas{framebuffer};
cadenza::AppRuntime runtime{cadenza::FramebufferProfile::TEmbed};
cadenza::idf::TEmbedDisplay display;
cadenza::idf::TEmbedInput input;
LogDiagnosticSink diagnostics;
cadenza::LauncherApp launcher;
cadenza::ClockApp clockApp;
cadenza::MotionApp motion;
cadenza::SettingsApp settings;
cadenza::AnimationGalleryApp gallery;
#else
VoiceAdapter voice;
#endif
#if CONFIG_CADENZA_T_EMBED_SPEAKER
#if !CONFIG_CADENZA_T_EMBED_RUNTIME
cadenza::audio::InteractionSoundService sound;
#endif
cadenza::idf::TEmbedSpeaker speaker;
#endif
#if CONFIG_CADENZA_T_EMBED_MICROPHONE
cadenza::idf::TEmbedMicrophone microphone{voice.capture};
#endif

esp_err_t provide_voice(uint8_t *buffer, size_t length, size_t *bytes_read,
                        void *context) {
  auto *adapter = static_cast<VoiceAdapter *>(context);
  adapter->sessionBridge.synchronize(adapter->packetizer);
  size_t offset = 0;
  while (offset < length) {
    cadenza::voice::UsbVoicePacket packet;
    if (!adapter->packetizer.nextPacket(packet)) {
      packet.samples.fill(0);
    }
    const size_t remaining = length - offset;
    const size_t copy = remaining < sizeof(packet.samples)
                            ? remaining
                            : sizeof(packet.samples);
    std::memcpy(buffer + offset, packet.samples.data(), copy);
    offset += copy;
  }
  *bytes_read = length;
  return ESP_OK;
}

#if !CONFIG_CADENZA_T_EMBED_MICROPHONE
void synthetic_capture_task(void *context) {
  auto *adapter = static_cast<VoiceAdapter *>(context);
  TickType_t last_wake = xTaskGetTickCount();
  while (true) {
    if (adapter->capture.state() == cadenza::VoiceCaptureState::Starting) {
      adapter->capture.notifyStarted(cadenza::voice::kVoicePcmFormat);
    }
    cadenza::voice::VoiceBlock block;
    block.sequence = adapter->sequence++;
    adapter->capture.publish(block, cadenza::voice::kVoicePcmFormat);
    vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
  }
}
#endif

#if CONFIG_CADENZA_T_EMBED_MICROPHONE
void hardware_capture_task(void *context) {
  static_cast<cadenza::idf::TEmbedMicrophone *>(context)->run();
  vTaskDelete(nullptr);
}
#endif

}  // namespace

extern "C" void tud_mount_cb(void) { cadenza::idf::uacMounted(); }
extern "C" void tud_umount_cb(void) { cadenza::idf::uacUnmounted(); }
extern "C" void tud_suspend_cb(bool remote_wakeup_enabled) {
  (void)remote_wakeup_enabled;
  cadenza::idf::uacSuspended();
}
extern "C" void tud_resume_cb(void) { cadenza::idf::uacResumed(); }

extern "C" void app_main(void) {
  initArduino();

#if CONFIG_CADENZA_T_EMBED_RUNTIME
  ESP_ERROR_CHECK(display.start());
  ESP_ERROR_CHECK(input.start());
  runtime.setDiagnosticSink(&diagnostics);
  services.setDiagnosticSink(&diagnostics);
  ESP_ERROR_CHECK(runtime.registerApp(cadenza::apps::kLauncherAppId, launcher,
                                      false,
                                      cadenza::apps::builtinAppCapabilities(
                                          cadenza::apps::kLauncherAppId))
                      ? ESP_OK
                      : ESP_FAIL);
  ESP_ERROR_CHECK(runtime.registerApp(
                      cadenza::apps::kClockAppId, clockApp, true,
                      cadenza::apps::builtinAppCapabilities(
                          cadenza::apps::kClockAppId))
                      ? ESP_OK
                      : ESP_FAIL);
  ESP_ERROR_CHECK(runtime.registerApp(
                      cadenza::apps::kMotionAppId, motion, true,
                      cadenza::apps::builtinAppCapabilities(
                          cadenza::apps::kMotionAppId))
                      ? ESP_OK
                      : ESP_FAIL);
  ESP_ERROR_CHECK(runtime.registerApp(
                      cadenza::apps::kSettingsAppId, settings, true,
                      cadenza::apps::builtinAppCapabilities(
                          cadenza::apps::kSettingsAppId))
                      ? ESP_OK
                      : ESP_FAIL);
  ESP_ERROR_CHECK(runtime.registerApp(
                      cadenza::apps::kGalleryAppId, gallery, true,
                      cadenza::apps::builtinAppCapabilities(
                          cadenza::apps::kGalleryAppId))
                      ? ESP_OK
                      : ESP_FAIL);
  ESP_ERROR_CHECK(runtime.configureHome(cadenza::apps::kLauncherAppId)
                      ? ESP_OK
                      : ESP_FAIL);
  ESP_ERROR_CHECK(runtime.begin(cadenza::apps::kLauncherAppId) ? ESP_OK
                                                               : ESP_FAIL);
  runtime.bindSystem(services.snapshot(), services);
#if CONFIG_CADENZA_CONNECTIVITY_RUNTIME
  ESP_ERROR_CHECK(connectivity.start());
#endif
#endif

#if CONFIG_CADENZA_T_EMBED_SPEAKER
#if CONFIG_CADENZA_T_EMBED_RUNTIME
  const esp_err_t speakerResult = speaker.start(services.sound());
  services.postPlatformEvent(cadenza::system::PlatformEvent::soundOutputAvailability(
      speakerResult == ESP_OK));
  ESP_ERROR_CHECK(speakerResult);
#else
  ESP_ERROR_CHECK(speaker.start(sound));
#endif
#endif

  voice.capture.setAvailable(true);
  cadenza::idf::installUacLifecycleAdapter(voice.sessionBridge, 1);
#if CONFIG_CADENZA_T_EMBED_MICROPHONE
  ESP_ERROR_CHECK(microphone.start());
  BaseType_t task_created = xTaskCreatePinnedToCore(
      hardware_capture_task, "cadenza_mic",
      cadenza::t_embed_audio::kMicrophone.taskStackBytes, &microphone,
      cadenza::t_embed_audio::kMicrophone.taskPriority, nullptr,
      cadenza::t_embed_audio::kMicrophone.taskCore);
#else
  BaseType_t task_created = xTaskCreatePinnedToCore(
      synthetic_capture_task, "cadenza_mic", 4096, &voice, 5, nullptr, 1);
#endif
  ESP_ERROR_CHECK(task_created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

  uac_device_config_t config = {
      .skip_tinyusb_init = false,
      .output_cb = nullptr,
      .input_cb = provide_voice,
      .set_mute_cb = nullptr,
      .set_volume_cb = nullptr,
      .cb_ctx = &voice,
      .spk_itf_num = -1,
      .mic_itf_num = 1,
  };
  ESP_ERROR_CHECK(uac_device_init(&config));

#if CONFIG_CADENZA_T_EMBED_RUNTIME
  std::int64_t lastFrameUs = esp_timer_get_time();
  while (true) {
    input.sample();
    const std::int64_t nowUs = esp_timer_get_time();
    if (nowUs - lastFrameUs >= 16'667) {
      const float dt = static_cast<float>(nowUs - lastFrameUs) / 1'000'000.0F;
      lastFrameUs = nowUs;
#if CONFIG_CADENZA_CONNECTIVITY_RUNTIME
      connectivity.pump();
#endif
      cadenza::system::FrameCoordinator::runFrame(
          services, runtime, canvas, dt > 0.05F ? 0.05F : dt,
          input.takeFrame());
#if CONFIG_CADENZA_CONNECTIVITY_RUNTIME
      connectivity.pump();
#endif
      if (!display.present(framebuffer)) {
        ESP_LOGE("cadenza_display", "frame transfer failed");
      }
    }
    vTaskDelay(pdMS_TO_TICKS(1));
  }
#endif
}
