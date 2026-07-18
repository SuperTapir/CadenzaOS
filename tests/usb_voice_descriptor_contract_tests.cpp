#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstdint>

#include "cadenza_usb_voice_contract.h"

namespace {
constexpr std::uint8_t endpointNumber(std::uint8_t address) {
  return address & 0x0FU;
}
constexpr bool endpointIn(std::uint8_t address) {
  return (address & 0x80U) != 0;
}
}  // namespace

TEST_CASE("candidate descriptor is UAC2 microphone-only at fixed PCM format") {
  CHECK(CADENZA_USB_VOICE_SAMPLE_RATE_HZ == 48000);
  CHECK(CADENZA_USB_VOICE_BITS_PER_SAMPLE == 16);
  CHECK(CADENZA_USB_VOICE_CHANNELS == 1);
  CHECK(CADENZA_USB_VOICE_INTERVAL_MS == 1);
  CHECK(CADENZA_USB_VOICE_NOMINAL_PACKET_BYTES == 48 * 2);
  CHECK(CADENZA_USB_VOICE_MAX_PACKET_BYTES == 49 * 2);
  CHECK(endpointIn(CADENZA_USB_EP_AUDIO_IN));
}

TEST_CASE("UAC2 and CDC composite interfaces and endpoint addresses are unique") {
  constexpr std::array<std::uint8_t, 4> interfaces{
      CADENZA_USB_ITF_AUDIO_CONTROL,
      CADENZA_USB_ITF_AUDIO_STREAMING_MIC,
      CADENZA_USB_ITF_CDC_CONTROL,
      CADENZA_USB_ITF_CDC_DATA,
  };
  for (std::size_t left = 0; left < interfaces.size(); ++left) {
    for (std::size_t right = left + 1; right < interfaces.size(); ++right) {
      CHECK(interfaces[left] != interfaces[right]);
    }
  }
  CHECK(CADENZA_USB_ITF_TOTAL == interfaces.size());

  constexpr std::array<std::uint8_t, 4> endpoints{
      CADENZA_USB_EP_AUDIO_IN,
      CADENZA_USB_EP_CDC_NOTIFICATION,
      CADENZA_USB_EP_CDC_OUT,
      CADENZA_USB_EP_CDC_IN,
  };
  for (std::size_t left = 0; left < endpoints.size(); ++left) {
    CHECK(endpointNumber(endpoints[left]) >= 1);
    CHECK(endpointNumber(endpoints[left]) <= 5);
    for (std::size_t right = left + 1; right < endpoints.size(); ++right) {
      CHECK(endpoints[left] != endpoints[right]);
    }
  }
  CHECK(endpointNumber(CADENZA_USB_EP_CDC_OUT) ==
        endpointNumber(CADENZA_USB_EP_CDC_IN));
  CHECK_FALSE(endpointIn(CADENZA_USB_EP_CDC_OUT));
  CHECK(endpointIn(CADENZA_USB_EP_CDC_IN));
}
