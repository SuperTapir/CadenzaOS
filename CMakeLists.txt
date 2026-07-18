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
target_compile_definitions(cadenza_u8g2 PUBLIC U8G2_WITHOUT_FONT_ROTATION)

add_library(cadenza_core STATIC
  lib/cadenza_core/src/input.cpp
  lib/cadenza_core/src/simulation_clock.cpp
  lib/cadenza_core/src/version.cpp
)
add_library(cadenza::core ALIAS cadenza_core)
target_include_directories(cadenza_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/lib/cadenza_core/include>
)
target_compile_features(cadenza_core PUBLIC cxx_std_17)
target_link_libraries(cadenza_core PRIVATE cadenza::u8g2)

if(CADENZA_BUILD_TESTS)
  enable_testing()

  add_executable(cadenza_legacy_core_compile_probe EXCLUDE_FROM_ALL
    tests/legacy_core_compile_probe.cpp
  )
  target_include_directories(cadenza_legacy_core_compile_probe PRIVATE include)

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
    NAME cadenza_legacy_core_is_not_portable_yet
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
      --target cadenza_legacy_core_compile_probe
  )
  set_tests_properties(cadenza_legacy_core_is_not_portable_yet PROPERTIES
    WILL_FAIL TRUE
  )
endif()

if(CADENZA_BUILD_DESKTOP)
  find_package(SDL3 CONFIG REQUIRED)
  add_executable(cadenza_desktop desktop/main.cpp)
  target_link_libraries(cadenza_desktop PRIVATE cadenza::core SDL3::SDL3)
endif()
