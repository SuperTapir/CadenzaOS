/* Build-only descriptor proof for one UAC2 microphone plus CDC. */
#include <string.h>

#include "tusb.h"
#include "uac_descriptors.h"
#include "cadenza_usb_voice_contract.h"

enum {
  ITF_NUM_AUDIO_CONTROL = CADENZA_USB_ITF_AUDIO_CONTROL,
  ITF_NUM_AUDIO_STREAMING_MIC = CADENZA_USB_ITF_AUDIO_STREAMING_MIC,
  ITF_NUM_CDC_CONTROL = CADENZA_USB_ITF_CDC_CONTROL,
  ITF_NUM_CDC_DATA = CADENZA_USB_ITF_CDC_DATA,
  ITF_NUM_TOTAL = CADENZA_USB_ITF_TOTAL,
};

enum {
  EPNUM_AUDIO_IN = CADENZA_USB_EP_AUDIO_IN,
  EPNUM_CDC_NOTIFICATION = CADENZA_USB_EP_CDC_NOTIFICATION,
  EPNUM_CDC_OUT = CADENZA_USB_EP_CDC_OUT,
  EPNUM_CDC_IN = CADENZA_USB_EP_CDC_IN,
};

static tusb_desc_device_t const device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = TUSB_CLASS_MISC,
    .bDeviceSubClass = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x303A,
    .idProduct = 0x8000,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

uint8_t const *tud_descriptor_device_cb(void) {
  return (uint8_t const *)&device_descriptor;
}

#define CONFIG_TOTAL_LENGTH \
  (TUD_CONFIG_DESC_LEN + TUD_AUDIO_DEVICE_DESC_LEN + TUD_CDC_DESC_LEN)

static uint8_t const configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LENGTH,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_AUDIO_DESCRIPTOR(ITF_NUM_AUDIO_CONTROL, 4, 0, EPNUM_AUDIO_IN, 0),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC_CONTROL, 6, EPNUM_CDC_NOTIFICATION, 8,
                       EPNUM_CDC_OUT, EPNUM_CDC_IN, 64),
};

uint8_t const *tud_descriptor_configuration_cb(uint8_t index) {
  (void)index;
  return configuration_descriptor;
}

static char const *const string_descriptors[] = {
    (const char[]){0x09, 0x04},
    "Cadenza",
    "Cadenza Voice Input Spike",
    "SPIKE-0001",
    "Cadenza microphone",
    "Microphone streaming",
    "Cadenza control",
};

static uint16_t string_buffer[32];

uint16_t const *tud_descriptor_string_cb(uint8_t index, uint16_t language_id) {
  (void)language_id;
  uint8_t count = 0;
  if (index == 0) {
    memcpy(&string_buffer[1], string_descriptors[0], 2);
    count = 1;
  } else {
    if (index >= TU_ARRAY_SIZE(string_descriptors)) {
      return NULL;
    }
    const char *value = string_descriptors[index];
    count = (uint8_t)strlen(value);
    if (count > 31) {
      count = 31;
    }
    for (uint8_t position = 0; position < count; ++position) {
      string_buffer[1 + position] = (uint8_t)value[position];
    }
  }
  string_buffer[0] = (uint16_t)((TUSB_DESC_STRING << 8) | (2 * count + 2));
  return string_buffer;
}
