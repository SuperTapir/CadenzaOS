#include "uac_lifecycle_adapter.h"

#include "tusb.h"

namespace {

cadenza::voice::UsbVoiceSessionBridge* lifecycle = nullptr;
std::uint8_t microphoneInterface = 0;

void applyAltSetting(const tusb_control_request_t* request,
                     bool closing) noexcept {
  if (lifecycle == nullptr || request == nullptr) return;
  const std::uint8_t interfaceNumber =
      tu_u16_low(tu_le16toh(request->wIndex));
  const std::uint8_t alternateSetting =
      tu_u16_low(tu_le16toh(request->wValue));
  if (interfaceNumber != microphoneInterface) return;
  lifecycle->onAltSettingFromCallback(!closing && alternateSetting != 0);
}

}  // namespace

namespace cadenza::idf {

void installUacLifecycleAdapter(voice::UsbVoiceSessionBridge& bridge,
                                std::uint8_t interfaceNumber) noexcept {
  lifecycle = &bridge;
  microphoneInterface = interfaceNumber;
}

void uacMounted() noexcept {
  if (lifecycle) lifecycle->onConnectedFromCallback(true);
}

void uacUnmounted() noexcept {
  if (lifecycle) lifecycle->onConnectedFromCallback(false);
}

void uacSuspended() noexcept { uacUnmounted(); }

void uacResumed() noexcept { uacMounted(); }

}  // namespace cadenza::idf

extern "C" bool cadenza_upstream_tud_audio_set_itf_cb(
    std::uint8_t rhport, const tusb_control_request_t* request);
extern "C" bool cadenza_upstream_tud_audio_set_itf_close_ep_cb(
    std::uint8_t rhport, const tusb_control_request_t* request);

extern "C" bool tud_audio_set_itf_cb(
    std::uint8_t rhport, const tusb_control_request_t* request) {
  const bool accepted =
      cadenza_upstream_tud_audio_set_itf_cb(rhport, request);
  if (accepted) applyAltSetting(request, false);
  return accepted;
}

extern "C" bool tud_audio_set_itf_close_ep_cb(
    std::uint8_t rhport, const tusb_control_request_t* request) {
  const bool accepted =
      cadenza_upstream_tud_audio_set_itf_close_ep_cb(rhport, request);
  if (accepted) applyAltSetting(request, true);
  return accepted;
}
