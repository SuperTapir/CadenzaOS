cmake_minimum_required(VERSION 3.20)

project(CadenzaOS VERSION 0.1.0 LANGUAGES C CXX)

option(CADENZA_BUILD_TESTS "Build host tests" ON)
option(CADENZA_BUILD_DESKTOP "Build the SDL3 desktop host" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(cadenza_u8g2 STATIC
  third_party/u8g2/src/u8g2_bitmap.c
  third_party/u8g2/src/u8g2_box.c
  third_party/u8g2/src/u8g2_buffer.c
  third_party/u8g2/src/u8g2_circle.c
  third_party/u8g2/src/u8g2_font.c
  third_party/u8g2/src/u8g2_font_6x10_tf.c
  third_party/u8g2/src/u8g2_hvline.c
  third_party/u8g2/src/u8g2_intersection.c
  third_party/u8g2/src/u8g2_kerning.c
  third_party/u8g2/src/u8g2_line.c
  third_party/u8g2/src/u8g2_ll_hvline.c
  third_party/u8g2/src/u8g2_setup.c
  third_party/u8g2/src/u8x8_8x8.c
  third_party/u8g2/src/u8x8_byte.c
  third_party/u8g2/src/u8x8_cad.c
  third_party/u8g2/src/u8x8_display.c
  third_party/u8g2/src/u8x8_gpio.c
  third_party/u8g2/src/u8x8_setup.c
  third_party/u8g2/src/u8x8_string.c
)
add_library(cadenza::u8g2 ALIAS cadenza_u8g2)
target_include_directories(cadenza_u8g2 PUBLIC third_party/u8g2/include)
target_compile_definitions(cadenza_u8g2 PUBLIC
  U8G2_WITHOUT_FONT_ROTATION
  U8X8_WITH_USER_PTR
)

include(cmake/cadenza_core_sources.cmake)
add_library(cadenza_core STATIC ${CADENZA_CORE_SOURCES})
add_library(cadenza::core ALIAS cadenza_core)
target_include_directories(cadenza_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/cadenza_core/include>
)
target_compile_features(cadenza_core PUBLIC cxx_std_17)
target_link_libraries(cadenza_core PRIVATE cadenza::u8g2)

include(cmake/cadenza_apps_sources.cmake)
add_library(cadenza_apps STATIC ${CADENZA_APPS_SOURCES})
add_library(cadenza::apps ALIAS cadenza_apps)
target_include_directories(cadenza_apps PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/cadenza_apps/include>
)
target_link_libraries(cadenza_apps PUBLIC cadenza::core)
target_compile_features(cadenza_apps PUBLIC cxx_std_17)

add_library(cadenza_system STATIC
  lib/cadenza_system/src/connectivity_service.cpp
  lib/cadenza_system/src/system_service_host.cpp
  lib/cadenza_system/src/system_resource_leases.cpp
  lib/cadenza_system/src/timer_service.cpp
  lib/cadenza_system/src/usb_voice_packetizer.cpp
  lib/cadenza_system/src/usb_voice_session_bridge.cpp
  lib/cadenza_system/src/voice_dma_normalizer.cpp
  lib/cadenza_system/src/voice_input.cpp
)
add_library(cadenza::system ALIAS cadenza_system)
target_include_directories(cadenza_system PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/cadenza_system/include>
)
target_link_libraries(cadenza_system PUBLIC cadenza::core)
target_compile_features(cadenza_system PUBLIC cxx_std_17)

add_library(cadenza_host STATIC
  lib/cadenza_host/src/headless_connectivity_adapter.cpp
  lib/cadenza_host/src/headless_host.cpp
  lib/cadenza_host/src/headless_microphone.cpp
)
add_library(cadenza::host ALIAS cadenza_host)
target_include_directories(cadenza_host PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/cadenza_host/include>
)
target_link_libraries(cadenza_host PUBLIC
  cadenza::core cadenza::apps cadenza::system
)
target_compile_features(cadenza_host PUBLIC cxx_std_17)

add_library(cadenza_desktop_model STATIC
  lib/cadenza_desktop/src/desktop_model.cpp
  lib/cadenza_desktop/src/png_writer.cpp
  lib/cadenza_desktop/src/recording.cpp
  lib/cadenza_desktop/src/simulation_controller.cpp
)
add_library(cadenza::desktop_model ALIAS cadenza_desktop_model)
target_include_directories(cadenza_desktop_model PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/cadenza_desktop/include>
)
target_include_directories(cadenza_desktop_model PRIVATE
  third_party/gif
  third_party/stb
)
target_link_libraries(cadenza_desktop_model PUBLIC cadenza::core)
target_compile_features(cadenza_desktop_model PUBLIC cxx_std_17)

if(CADENZA_BUILD_TESTS)
  enable_testing()
  find_package(Threads REQUIRED)
  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -c "import PIL"
    RESULT_VARIABLE CADENZA_PYTHON_HAS_PILLOW
    OUTPUT_QUIET ERROR_QUIET
  )

  if(CADENZA_PYTHON_HAS_PILLOW EQUAL 0)
    add_test(
      NAME cadenza_font_converter_tests
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/font_converter_tests.py
    )
    add_test(
      NAME cadenza_roobert_font_asset_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/convert_playdate_fonts.py
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/roobert-source
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/cadenza_core/src/generated/roobert_fonts.h
        --check
    )
  endif()

  add_test(
    NAME cadenza_timer_digit_source_check
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/generate_timer_digits.py --check
  )
  add_test(
    NAME cadenza_timer_digit_t_embed_asset_check
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/pack_bitmap.py
      ${CMAKE_CURRENT_SOURCE_DIR}/assets/timer-digits/timer_digits_t_embed.pbm
      ${CMAKE_CURRENT_SOURCE_DIR}/lib/cadenza_core/include/cadenza/presentation/generated/timer_digits_t_embed.h
      --symbol kTimerDigitsTEmbed
      --provenance "Cadenza-authored industrial Timer numerals; native 528x84 atlas from tools/generate_timer_digits.py"
      --check
  )
  add_test(
    NAME cadenza_timer_digit_sharp_asset_check
    COMMAND ${Python3_EXECUTABLE}
      ${CMAKE_CURRENT_SOURCE_DIR}/tools/pack_bitmap.py
      ${CMAKE_CURRENT_SOURCE_DIR}/assets/timer-digits/timer_digits_sharp.pbm
      ${CMAKE_CURRENT_SOURCE_DIR}/lib/cadenza_core/include/cadenza/presentation/generated/timer_digits_sharp.h
      --symbol kTimerDigitsSharp
      --provenance "Cadenza-authored industrial Timer numerals; native 716x120 atlas from tools/generate_timer_digits.py"
      --check
  )

  foreach(launcher_cover IN ITEMS timer motion settings gallery)
    if(launcher_cover STREQUAL "timer")
      set(launcher_cover_threshold 150)
    elseif(launcher_cover STREQUAL "motion")
      set(launcher_cover_threshold 170)
    elseif(launcher_cover STREQUAL "settings")
      set(launcher_cover_threshold 165)
    else()
      set(launcher_cover_threshold 155)
    endif()
    string(SUBSTRING "${launcher_cover}" 0 1 launcher_cover_initial)
    string(TOUPPER "${launcher_cover_initial}" launcher_cover_initial)
    string(SUBSTRING "${launcher_cover}" 1 -1 launcher_cover_tail)
    set(launcher_cover_symbol
      "k${launcher_cover_initial}${launcher_cover_tail}Cover")
    if(launcher_cover STREQUAL "timer")
      set(launcher_cover_provenance
        "Cadenza original AI-assisted TIMER interlock artwork, user-selected 2026-07-19; canonical PBM in assets/launcher-covers/timer.pbm")
      set(launcher_cover_t_embed_provenance
        "Cadenza original AI-assisted TIMER interlock artwork, user-selected 2026-07-19; direct 280x124 derivative of assets/launcher-covers/source/timer.png")
    else()
      set(launcher_cover_provenance
        "Cadenza original AI-assisted artwork, user-approved 2026-07-18; canonical PBM in assets/launcher-covers/${launcher_cover}.pbm")
      set(launcher_cover_t_embed_provenance
        "Cadenza original AI-assisted artwork; direct 280x124 derivative of assets/launcher-covers/source/${launcher_cover}.png")
    endif()
    if(CADENZA_PYTHON_HAS_PILLOW EQUAL 0)
      if(launcher_cover STREQUAL "timer")
        add_test(
          NAME cadenza_launcher_timer_cover_source_check
          COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/.codex/skills/generate-launcher-cover-art/scripts/prepare_cover.py
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher-covers/source/timer.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher-covers
            --name timer --levels 5
            --solid-light-region 0.026 0.40 0.70 0.92 --check
        )
      else()
        add_test(
          NAME cadenza_launcher_${launcher_cover}_cover_source_check
          COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/convert_launcher_cover.py
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher-covers/source/${launcher_cover}.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher-covers/${launcher_cover}.pbm
            --width 350 --height 155 --threshold ${launcher_cover_threshold}
            --check
        )
      endif()
    endif()
    add_test(
      NAME cadenza_launcher_${launcher_cover}_cover_asset_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/pack_bitmap.py
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher-covers/${launcher_cover}.pbm
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/cadenza_apps/src/generated/${launcher_cover}_cover.h
        --symbol ${launcher_cover_symbol}
        --provenance "${launcher_cover_provenance}"
        --check
    )
    if(CADENZA_PYTHON_HAS_PILLOW EQUAL 0)
      if(launcher_cover STREQUAL "timer")
        add_test(
          NAME cadenza_launcher_timer_t_embed_cover_source_check
          COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/.codex/skills/generate-launcher-cover-art/scripts/prepare_cover.py
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher-covers/source/timer.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher-covers
            --name timer --levels 5
            --solid-light-region 0.026 0.40 0.70 0.92 --check
        )
      else()
        add_test(
          NAME cadenza_launcher_${launcher_cover}_t_embed_cover_source_check
          COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tools/convert_launcher_cover.py
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher-covers/source/${launcher_cover}.png
            ${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher-covers/${launcher_cover}_t_embed.pbm
            --width 280 --height 124 --threshold ${launcher_cover_threshold}
            --check
        )
      endif()
    endif()
    add_test(
      NAME cadenza_launcher_${launcher_cover}_t_embed_cover_asset_check
      COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/pack_bitmap.py
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/launcher-covers/${launcher_cover}_t_embed.pbm
        ${CMAKE_CURRENT_SOURCE_DIR}/lib/cadenza_apps/src/generated/${launcher_cover}_t_embed_cover.h
        --symbol "k${launcher_cover_initial}${launcher_cover_tail}TEmbedCover"
        --provenance "${launcher_cover_t_embed_provenance}"
        --check
    )
  endforeach()

  add_executable(cadenza_portable_core_compile_probe
    tests/legacy_core_compile_probe.cpp
  )
  target_link_libraries(cadenza_portable_core_compile_probe PRIVATE cadenza::core)

  add_executable(cadenza_core_smoke tests/core_smoke.cpp)
  target_link_libraries(cadenza_core_smoke PRIVATE cadenza::core)
  add_test(NAME cadenza_core_smoke COMMAND cadenza_core_smoke)

  add_executable(cadenza_core_services_tests tests/core_services_tests.cpp)
  target_link_libraries(cadenza_core_services_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_core_services_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_core_services_tests COMMAND cadenza_core_services_tests)

  add_executable(cadenza_input_tests tests/input_reducer_tests.cpp)
  target_link_libraries(cadenza_input_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_input_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_input_tests COMMAND cadenza_input_tests)

  add_executable(cadenza_audio_engine_tests tests/audio_engine_tests.cpp)
  target_link_libraries(cadenza_audio_engine_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_audio_engine_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_audio_engine_tests COMMAND cadenza_audio_engine_tests)

  add_executable(cadenza_audio_command_queue_tests
    tests/audio_command_queue_tests.cpp
  )
  target_link_libraries(cadenza_audio_command_queue_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_audio_command_queue_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_audio_command_queue_tests
    COMMAND cadenza_audio_command_queue_tests
  )

  add_executable(cadenza_interaction_sound_tests
    tests/interaction_sound_tests.cpp
  )
  target_link_libraries(cadenza_interaction_sound_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_interaction_sound_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_interaction_sound_tests
    COMMAND cadenza_interaction_sound_tests
  )

  add_executable(cadenza_i2s_audio_format_tests
    tests/i2s_audio_format_tests.cpp
  )
  target_link_libraries(cadenza_i2s_audio_format_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_i2s_audio_format_tests PRIVATE
    include third_party/doctest
  )
  add_test(NAME cadenza_i2s_audio_format_tests
    COMMAND cadenza_i2s_audio_format_tests
  )

  add_executable(cadenza_i2s_audio_output_tests
    tests/i2s_audio_output_tests.cpp
    tests/firmware_stubs/firmware_stubs.cpp
    src/i2s_audio_output.cpp
  )
  target_link_libraries(cadenza_i2s_audio_output_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_i2s_audio_output_tests PRIVATE
    tests/firmware_stubs include third_party/doctest
  )
  add_test(NAME cadenza_i2s_audio_output_tests
    COMMAND cadenza_i2s_audio_output_tests
  )

  add_executable(cadenza_audio_io_concurrency_tests
    tests/audio_io_concurrency_tests.cpp
  )
  target_link_libraries(cadenza_audio_io_concurrency_tests PRIVATE
    cadenza::core cadenza::system
  )
  target_include_directories(cadenza_audio_io_concurrency_tests PRIVATE
    include third_party/doctest
  )
  add_test(NAME cadenza_audio_io_concurrency_tests
    COMMAND cadenza_audio_io_concurrency_tests
  )

  add_executable(cadenza_framebuffer_tests tests/framebuffer_tests.cpp)
  target_link_libraries(cadenza_framebuffer_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_framebuffer_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_framebuffer_tests COMMAND cadenza_framebuffer_tests)

  add_executable(cadenza_canvas_tests tests/mono_canvas_tests.cpp)
  target_link_libraries(cadenza_canvas_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_canvas_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_canvas_tests COMMAND cadenza_canvas_tests)

  add_executable(cadenza_bounded_text_tests tests/bounded_text_tests.cpp)
  target_link_libraries(cadenza_bounded_text_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_bounded_text_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_bounded_text_tests COMMAND cadenza_bounded_text_tests)

  add_executable(cadenza_sprite_atlas_tests tests/sprite_atlas_tests.cpp)
  target_link_libraries(cadenza_sprite_atlas_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_sprite_atlas_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_sprite_atlas_tests COMMAND cadenza_sprite_atlas_tests)

  add_executable(cadenza_app_runtime_tests tests/app_runtime_tests.cpp)
  target_link_libraries(cadenza_app_runtime_tests PRIVATE
    cadenza::core cadenza::system
  )
  target_include_directories(cadenza_app_runtime_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_app_runtime_tests COMMAND cadenza_app_runtime_tests)

  add_executable(cadenza_app_handoff_allocation_tests
    tests/app_handoff_allocation_tests.cpp
  )
  target_link_libraries(cadenza_app_handoff_allocation_tests PRIVATE
    cadenza::apps cadenza::core cadenza::system
  )
  target_include_directories(cadenza_app_handoff_allocation_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_app_handoff_allocation_tests
    COMMAND cadenza_app_handoff_allocation_tests
  )

  add_executable(cadenza_system_surface_tests tests/system_surface_tests.cpp)
  target_link_libraries(cadenza_system_surface_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_system_surface_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_system_surface_tests COMMAND cadenza_system_surface_tests)

  add_executable(cadenza_app_context_contract_tests
    tests/app_context_contract_tests.cpp
  )
  target_link_libraries(cadenza_app_context_contract_tests PRIVATE
    cadenza::core cadenza::system
  )
  target_include_directories(cadenza_app_context_contract_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_app_context_contract_tests
    COMMAND cadenza_app_context_contract_tests
  )

  add_executable(cadenza_system_service_host_tests
    tests/system_service_host_tests.cpp
  )
  target_link_libraries(cadenza_system_service_host_tests PRIVATE
    cadenza::system
  )
  target_include_directories(cadenza_system_service_host_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_system_service_host_tests
    COMMAND cadenza_system_service_host_tests
  )

  add_executable(cadenza_timer_service_tests
    tests/timer_service_tests.cpp
  )
  target_link_libraries(cadenza_timer_service_tests PRIVATE
    cadenza::system
  )
  target_include_directories(cadenza_timer_service_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_timer_service_tests
    COMMAND cadenza_timer_service_tests
  )

  add_executable(cadenza_connectivity_boundary_tests
    tests/connectivity_boundary_tests.cpp
  )
  target_link_libraries(cadenza_connectivity_boundary_tests PRIVATE
    cadenza::system Threads::Threads
  )
  target_include_directories(cadenza_connectivity_boundary_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_connectivity_boundary_tests
    COMMAND cadenza_connectivity_boundary_tests
  )

  add_executable(cadenza_voice_input_core_tests
    tests/voice_input_core_tests.cpp
  )
  target_link_libraries(cadenza_voice_input_core_tests PRIVATE
    cadenza::system
  )
  target_include_directories(cadenza_voice_input_core_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_voice_input_core_tests
    COMMAND cadenza_voice_input_core_tests
  )

  add_executable(cadenza_usb_voice_packetizer_tests
    tests/usb_voice_packetizer_tests.cpp
  )
  target_link_libraries(cadenza_usb_voice_packetizer_tests PRIVATE
    cadenza::system
  )
  target_include_directories(cadenza_usb_voice_packetizer_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_usb_voice_packetizer_tests
    COMMAND cadenza_usb_voice_packetizer_tests
  )

  add_executable(cadenza_usb_voice_session_bridge_tests
    tests/usb_voice_session_bridge_tests.cpp
  )
  target_link_libraries(cadenza_usb_voice_session_bridge_tests PRIVATE
    cadenza::system
  )
  target_include_directories(cadenza_usb_voice_session_bridge_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_usb_voice_session_bridge_tests
    COMMAND cadenza_usb_voice_session_bridge_tests
  )

  add_executable(cadenza_voice_dma_normalizer_tests
    tests/voice_dma_normalizer_tests.cpp
  )
  target_link_libraries(cadenza_voice_dma_normalizer_tests PRIVATE
    cadenza::system
  )
  target_include_directories(cadenza_voice_dma_normalizer_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_voice_dma_normalizer_tests
    COMMAND cadenza_voice_dma_normalizer_tests
  )

  add_executable(cadenza_usb_voice_descriptor_contract_tests
    tests/usb_voice_descriptor_contract_tests.cpp
  )
  target_include_directories(cadenza_usb_voice_descriptor_contract_tests PRIVATE
    include third_party/doctest
  )
  add_test(NAME cadenza_usb_voice_descriptor_contract_tests
    COMMAND cadenza_usb_voice_descriptor_contract_tests
  )

  add_executable(cadenza_transition_runtime_tests tests/transition_runtime_tests.cpp)
  target_link_libraries(cadenza_transition_runtime_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_transition_runtime_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_transition_runtime_tests COMMAND cadenza_transition_runtime_tests)

  add_executable(cadenza_apps_tests tests/apps_tests.cpp)
  target_link_libraries(cadenza_apps_tests PRIVATE
    cadenza::apps cadenza::system
  )
  target_include_directories(cadenza_apps_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_apps_tests COMMAND cadenza_apps_tests)

  add_executable(cadenza_presenter_tests tests/presenter_tests.cpp)
  target_link_libraries(cadenza_presenter_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_presenter_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_presenter_tests COMMAND cadenza_presenter_tests)

  add_executable(cadenza_headless_host_tests tests/headless_host_tests.cpp)
  target_link_libraries(cadenza_headless_host_tests PRIVATE cadenza::host)
  target_include_directories(cadenza_headless_host_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_headless_host_tests COMMAND cadenza_headless_host_tests)

  add_executable(cadenza_headless_microphone_tests
    tests/headless_microphone_tests.cpp
  )
  target_link_libraries(cadenza_headless_microphone_tests PRIVATE
    cadenza::host
  )
  target_include_directories(cadenza_headless_microphone_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_headless_microphone_tests
    COMMAND cadenza_headless_microphone_tests
  )

  add_executable(cadenza_app_snapshot_tests tests/app_snapshot_tests.cpp)
  target_link_libraries(cadenza_app_snapshot_tests PRIVATE
    cadenza::host cadenza::desktop_model
  )
  target_include_directories(cadenza_app_snapshot_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_app_snapshot_tests COMMAND cadenza_app_snapshot_tests)

  add_executable(cadenza_launcher_snapshot_tests
    tests/launcher_snapshot_tests.cpp
  )
  target_link_libraries(cadenza_launcher_snapshot_tests PRIVATE
    cadenza::host cadenza::desktop_model
  )
  target_include_directories(cadenza_launcher_snapshot_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_launcher_snapshot_tests
    COMMAND cadenza_launcher_snapshot_tests
  )

  add_executable(cadenza_dump_headless_pbm tools/dump_headless_pbm.cpp)
  target_link_libraries(cadenza_dump_headless_pbm PRIVATE cadenza::host)

  add_executable(cadenza_dump_sound_cues tools/dump_sound_cues.cpp)
  target_link_libraries(cadenza_dump_sound_cues PRIVATE cadenza::core)

  add_executable(cadenza_system_overlay_size_probe
    tools/system_overlay_size_probe.cpp
  )
  target_link_libraries(cadenza_system_overlay_size_probe PRIVATE
    cadenza::core cadenza::system
  )
  add_test(NAME cadenza_system_overlay_size_probe
    COMMAND cadenza_system_overlay_size_probe
  )

  add_executable(cadenza_desktop_model_tests tests/desktop_model_tests.cpp)
  target_link_libraries(cadenza_desktop_model_tests PRIVATE
    cadenza::desktop_model cadenza::host
  )
  target_include_directories(cadenza_desktop_model_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_desktop_model_tests COMMAND cadenza_desktop_model_tests)

  add_executable(cadenza_desktop_smoke_tests tests/desktop_smoke_tests.cpp)
  target_link_libraries(cadenza_desktop_smoke_tests PRIVATE
    cadenza::desktop_model cadenza::host
  )
  target_include_directories(cadenza_desktop_smoke_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_desktop_smoke_tests COMMAND cadenza_desktop_smoke_tests)

  add_executable(cadenza_simulation_controller_tests
    tests/simulation_controller_tests.cpp
  )
  target_link_libraries(cadenza_simulation_controller_tests PRIVATE
    cadenza::desktop_model
  )
  target_include_directories(cadenza_simulation_controller_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_simulation_controller_tests
    COMMAND cadenza_simulation_controller_tests
  )

  find_package(PNG REQUIRED)
  add_executable(cadenza_png_writer_tests tests/png_writer_tests.cpp)
  target_link_libraries(cadenza_png_writer_tests PRIVATE
    cadenza::desktop_model PNG::PNG
  )
  target_include_directories(cadenza_png_writer_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_png_writer_tests COMMAND cadenza_png_writer_tests)

  add_executable(cadenza_recording_tests tests/recording_tests.cpp)
  target_link_libraries(cadenza_recording_tests PRIVATE cadenza::desktop_model)
  target_include_directories(cadenza_recording_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_recording_tests COMMAND cadenza_recording_tests)

  add_executable(cadenza_easing_tests tests/easing_tests.cpp)
  target_link_libraries(cadenza_easing_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_easing_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_easing_tests COMMAND cadenza_easing_tests)

  add_executable(cadenza_tween_tests tests/tween_tests.cpp)
  target_link_libraries(cadenza_tween_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_tween_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_tween_tests COMMAND cadenza_tween_tests)

  add_executable(cadenza_composition_tests tests/composition_tests.cpp)
  target_link_libraries(cadenza_composition_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_composition_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_composition_tests COMMAND cadenza_composition_tests)

  add_executable(cadenza_spring_tests tests/spring_tests.cpp)
  target_link_libraries(cadenza_spring_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_spring_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_spring_tests COMMAND cadenza_spring_tests)

  add_executable(cadenza_transition_set_tests tests/transition_set_tests.cpp)
  target_link_libraries(cadenza_transition_set_tests PRIVATE cadenza::host)
  target_include_directories(cadenza_transition_set_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_transition_set_tests COMMAND cadenza_transition_set_tests)

  add_executable(cadenza_presentation_effects_tests
    tests/presentation_effects_tests.cpp
  )
  target_link_libraries(cadenza_presentation_effects_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_presentation_effects_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_presentation_effects_tests
    COMMAND cadenza_presentation_effects_tests
  )

  add_executable(cadenza_particle_tests tests/particle_tests.cpp)
  target_link_libraries(cadenza_particle_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_particle_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_particle_tests COMMAND cadenza_particle_tests)

  add_executable(cadenza_frame_animation_tests tests/frame_animation_tests.cpp)
  target_link_libraries(cadenza_frame_animation_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_frame_animation_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_frame_animation_tests COMMAND cadenza_frame_animation_tests)

  add_executable(cadenza_animation_state_machine_tests
    tests/animation_state_machine_tests.cpp
  )
  target_link_libraries(cadenza_animation_state_machine_tests PRIVATE
    cadenza::core
  )
  target_include_directories(cadenza_animation_state_machine_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_animation_state_machine_tests
    COMMAND cadenza_animation_state_machine_tests
  )

  add_executable(cadenza_gallery_tests tests/gallery_tests.cpp)
  target_link_libraries(cadenza_gallery_tests PRIVATE
    cadenza::apps cadenza::system
  )
  target_include_directories(cadenza_gallery_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_gallery_tests COMMAND cadenza_gallery_tests)

  add_executable(cadenza_gallery_snapshot_tests
    tests/gallery_snapshot_tests.cpp
  )
  target_link_libraries(cadenza_gallery_snapshot_tests PRIVATE
    cadenza::host cadenza::desktop_model
  )
  target_include_directories(cadenza_gallery_snapshot_tests PRIVATE
    third_party/doctest
  )
  add_test(NAME cadenza_gallery_snapshot_tests
    COMMAND cadenza_gallery_snapshot_tests
  )

  add_executable(cadenza_vendor_smoke tests/vendor_smoke.cpp)
  target_link_libraries(cadenza_vendor_smoke PRIVATE cadenza::u8g2)
  target_include_directories(cadenza_vendor_smoke PRIVATE third_party/doctest)
  add_test(NAME cadenza_vendor_smoke COMMAND cadenza_vendor_smoke)

  add_test(
    NAME cadenza_third_party_notices
    COMMAND ${CMAKE_COMMAND}
      -DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/verify_third_party.cmake
  )

  add_test(
    NAME cadenza_shared_source_audit
    COMMAND ${CMAKE_COMMAND}
      -DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/audit_shared_sources.cmake
  )

  add_test(
    NAME cadenza_connectivity_research_audit
    COMMAND ${CMAKE_COMMAND}
      -DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/audit_connectivity_research.cmake
  )

  add_test(
    NAME cadenza_system_overlay_research_audit
    COMMAND ${CMAKE_COMMAND}
      -DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/audit_system_overlay_research.cmake
  )

  add_test(
    NAME cadenza_activation_timer_research_audit
    COMMAND ${CMAKE_COMMAND}
      -DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/audit_activation_timer_research.cmake
  )

  add_test(
    NAME cadenza_timer_naming_audit
    COMMAND ${CMAKE_COMMAND}
      -DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/audit_timer_naming.cmake
  )

  add_test(
    NAME cadenza_system_typography_source_audit
    COMMAND ${CMAKE_COMMAND}
      -DPROJECT_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
      -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/audit_system_typography.cmake
  )

  add_test(
    NAME cadenza_portable_core_compile_probe
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
      --target cadenza_portable_core_compile_probe
  )
endif()

if(CADENZA_BUILD_DESKTOP)
  find_package(SDL3 CONFIG REQUIRED)
  add_library(cadenza_sdl_audio STATIC
    lib/cadenza_desktop/src/sdl_audio_output.cpp
  )
  add_library(cadenza::sdl_audio ALIAS cadenza_sdl_audio)
  target_include_directories(cadenza_sdl_audio PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/cadenza_desktop/include>
  )
  target_link_libraries(cadenza_sdl_audio PUBLIC cadenza::core SDL3::SDL3)
  target_compile_features(cadenza_sdl_audio PUBLIC cxx_std_17)

  add_executable(cadenza_desktop desktop/main.cpp)
  target_link_libraries(cadenza_desktop PRIVATE
    cadenza::host cadenza::desktop_model cadenza::sdl_audio SDL3::SDL3
  )

  if(CADENZA_BUILD_TESTS)
    add_executable(cadenza_sdl_audio_output_tests
      tests/sdl_audio_output_tests.cpp
    )
    target_link_libraries(cadenza_sdl_audio_output_tests PRIVATE
      cadenza::sdl_audio
    )
    target_include_directories(cadenza_sdl_audio_output_tests PRIVATE
      third_party/doctest
    )
    add_test(NAME cadenza_sdl_audio_output_tests
      COMMAND cadenza_sdl_audio_output_tests
    )
    set_tests_properties(cadenza_sdl_audio_output_tests PROPERTIES
      ENVIRONMENT "SDL_AUDIODRIVER=dummy"
    )
  endif()
endif()
