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

add_library(cadenza_host STATIC
  lib/cadenza_host/src/headless_host.cpp
)
add_library(cadenza::host ALIAS cadenza_host)
target_include_directories(cadenza_host PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/cadenza_host/include>
)
target_link_libraries(cadenza_host PUBLIC cadenza::core)
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
  target_link_libraries(cadenza_app_runtime_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_app_runtime_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_app_runtime_tests COMMAND cadenza_app_runtime_tests)

  add_executable(cadenza_transition_runtime_tests tests/transition_runtime_tests.cpp)
  target_link_libraries(cadenza_transition_runtime_tests PRIVATE cadenza::core)
  target_include_directories(cadenza_transition_runtime_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_transition_runtime_tests COMMAND cadenza_transition_runtime_tests)

  add_executable(cadenza_apps_tests tests/apps_tests.cpp)
  target_link_libraries(cadenza_apps_tests PRIVATE cadenza::core)
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

  add_executable(cadenza_app_snapshot_tests tests/app_snapshot_tests.cpp)
  target_link_libraries(cadenza_app_snapshot_tests PRIVATE
    cadenza::host cadenza::desktop_model
  )
  target_include_directories(cadenza_app_snapshot_tests PRIVATE third_party/doctest)
  add_test(NAME cadenza_app_snapshot_tests COMMAND cadenza_app_snapshot_tests)

  add_executable(cadenza_dump_headless_pbm tools/dump_headless_pbm.cpp)
  target_link_libraries(cadenza_dump_headless_pbm PRIVATE cadenza::host)

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
  target_link_libraries(cadenza_gallery_tests PRIVATE cadenza::core)
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
    NAME cadenza_portable_core_compile_probe
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
      --target cadenza_portable_core_compile_probe
  )
endif()

if(CADENZA_BUILD_DESKTOP)
  find_package(SDL3 CONFIG REQUIRED)
  add_executable(cadenza_desktop desktop/main.cpp)
  target_link_libraries(cadenza_desktop PRIVATE
    cadenza::host cadenza::desktop_model SDL3::SDL3
  )
endif()
