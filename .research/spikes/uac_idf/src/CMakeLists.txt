set(cadenza_root "${CMAKE_CURRENT_LIST_DIR}/../../../..")

set(cadenza_runtime_sources)
set(cadenza_connectivity_sources)
if(CONFIG_CADENZA_T_EMBED_RUNTIME)
  list(APPEND cadenza_runtime_sources
    "t_embed_display.cpp"
    "t_embed_input.cpp"
    "${cadenza_root}/lib/cadenza_core/src/app_runtime.cpp"
    "${cadenza_root}/lib/cadenza_core/src/easing.cpp"
    "${cadenza_root}/lib/cadenza_core/src/effects.cpp"
    "${cadenza_root}/lib/cadenza_core/src/input.cpp"
    "${cadenza_root}/lib/cadenza_core/src/mono_canvas.cpp"
    "${cadenza_root}/lib/cadenza_core/src/mono_framebuffer.cpp"
    "${cadenza_root}/lib/cadenza_core/src/simulation_clock.cpp"
    "${cadenza_root}/lib/cadenza_core/src/spring.cpp"
    "${cadenza_root}/lib/cadenza_core/src/system_surface.cpp"
    "${cadenza_root}/lib/cadenza_core/src/system_typography.cpp"
    "${cadenza_root}/lib/cadenza_core/src/timer_numerals.cpp"
    "${cadenza_root}/lib/cadenza_core/src/transition.cpp"
    "${cadenza_root}/lib/cadenza_core/src/version.cpp"
    "${cadenza_root}/lib/cadenza_apps/src/apps.cpp"
    "${cadenza_root}/lib/cadenza_apps/src/gallery.cpp"
    "${cadenza_root}/lib/cadenza_apps/src/sight.cpp"
    "${cadenza_root}/lib/cadenza_system/src/system_service_host.cpp"
    "${cadenza_root}/lib/cadenza_system/src/system_resource_leases.cpp"
    "${cadenza_root}/lib/cadenza_system/src/connectivity_service.cpp"
    "${cadenza_root}/lib/cadenza_system/src/timer_service.cpp"
    "${cadenza_root}/third_party/u8g2/src/u8g2_bitmap.c"
    "${cadenza_root}/third_party/u8g2/src/u8g2_box.c"
    "${cadenza_root}/third_party/u8g2/src/u8g2_buffer.c"
    "${cadenza_root}/third_party/u8g2/src/u8g2_circle.c"
    "${cadenza_root}/third_party/u8g2/src/u8g2_font.c"
    "${cadenza_root}/third_party/u8g2/src/u8g2_font_6x10_tf.c"
    "${cadenza_root}/third_party/u8g2/src/u8g2_hvline.c"
    "${cadenza_root}/third_party/u8g2/src/u8g2_intersection.c"
    "${cadenza_root}/third_party/u8g2/src/u8g2_kerning.c"
    "${cadenza_root}/third_party/u8g2/src/u8g2_line.c"
    "${cadenza_root}/third_party/u8g2/src/u8g2_ll_hvline.c"
    "${cadenza_root}/third_party/u8g2/src/u8g2_setup.c"
    "${cadenza_root}/third_party/u8g2/src/u8x8_8x8.c"
    "${cadenza_root}/third_party/u8g2/src/u8x8_byte.c"
    "${cadenza_root}/third_party/u8g2/src/u8x8_cad.c"
    "${cadenza_root}/third_party/u8g2/src/u8x8_display.c"
    "${cadenza_root}/third_party/u8g2/src/u8x8_gpio.c"
    "${cadenza_root}/third_party/u8g2/src/u8x8_setup.c"
    "${cadenza_root}/third_party/u8g2/src/u8x8_string.c")
endif()

if(CONFIG_CADENZA_CONNECTIVITY_RUNTIME)
  list(APPEND cadenza_connectivity_sources
    "idf_connectivity_adapter.cpp")
endif()

idf_component_register(
  SRCS
    "main.cpp"
    "t_embed_microphone.cpp"
    "t_embed_speaker.cpp"
    "uac_lifecycle_adapter.cpp"
    "${cadenza_root}/lib/cadenza_core/src/audio_engine.cpp"
    "${cadenza_root}/lib/cadenza_core/src/interaction_sound.cpp"
    "${cadenza_root}/lib/cadenza_core/src/sound_cue_library.cpp"
    "${cadenza_root}/lib/cadenza_system/src/usb_voice_packetizer.cpp"
    "${cadenza_root}/lib/cadenza_system/src/usb_voice_session_bridge.cpp"
    "${cadenza_root}/lib/cadenza_system/src/voice_dma_normalizer.cpp"
    "${cadenza_root}/lib/cadenza_system/src/voice_dma_layout.cpp"
    "${cadenza_root}/lib/cadenza_system/src/voice_input.cpp"
    ${cadenza_runtime_sources}
    ${cadenza_connectivity_sources}
  INCLUDE_DIRS
    "."
    "${cadenza_root}/lib/cadenza_core/include"
    "${cadenza_root}/lib/cadenza_system/include"
    "${cadenza_root}/lib/cadenza_apps/include"
    "${cadenza_root}/include"
    "${cadenza_root}/third_party/u8g2/include"
  PRIV_REQUIRES
    espressif__arduino-esp32
    espressif__esp_codec_dev
    espressif__tinyusb
    espressif__usb_device_uac
    esp_driver_i2c
    esp_driver_i2s
    esp_driver_gpio
    esp_driver_spi
    driver
    esp_lcd
    esp_timer
    bt
    esp_event
    esp_netif
    esp_wifi
    nvs_flash
    wifi_provisioning)

idf_component_get_property(tinyusb_lib espressif__tinyusb COMPONENT_LIB)
idf_component_get_property(uac_lib espressif__usb_device_uac COMPONENT_LIB)
idf_component_get_property(uac_dir espressif__usb_device_uac COMPONENT_DIR)
target_sources(${tinyusb_lib} PRIVATE
  "${CMAKE_CURRENT_LIST_DIR}/usb_descriptors.c")
target_include_directories(${tinyusb_lib} PUBLIC
  "${CMAKE_CURRENT_LIST_DIR}"
  "${cadenza_root}/include"
  "${uac_dir}/tusb_uac")
target_include_directories(${COMPONENT_LIB} PRIVATE
  "${uac_dir}/tusb_uac")
target_compile_definitions(${COMPONENT_LIB} PRIVATE
  U8G2_WITHOUT_FONT_ROTATION
  U8X8_WITH_USER_PTR)

# usb_device_uac 1.3.1 owns TinyUSB's public SET_INTERFACE callbacks but does
# not expose lifecycle notifications. Rename its two callback definitions at
# compile time, then let the adapter below publish the public symbols and
# delegate to the unchanged upstream implementation first.
target_compile_definitions(${uac_lib} PRIVATE
  tud_audio_set_itf_cb=cadenza_upstream_tud_audio_set_itf_cb
  tud_audio_set_itf_close_ep_cb=cadenza_upstream_tud_audio_set_itf_close_ep_cb)
