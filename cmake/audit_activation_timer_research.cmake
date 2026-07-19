if(NOT DEFINED PROJECT_ROOT)
  message(FATAL_ERROR "PROJECT_ROOT is required")
endif()

set(research_doc "${PROJECT_ROOT}/docs/activation-timer-reference-research.md")
set(baseline_doc "${PROJECT_ROOT}/docs/activation-timer-baseline.md")
foreach(document IN ITEMS "${research_doc}" "${baseline_doc}")
  if(NOT EXISTS "${document}")
    message(FATAL_ERROR "Missing Activation Timer evidence: ${document}")
  endif()
endforeach()

file(READ "${research_doc}" research_text)
set(required_evidence
  "76ccf26b51403838ae8890e8ba286dea85d53c18"
  "684c9e8f32e4373a21098559f748f06915f950c9"
  "9b777ae5c5b8e9e456065a00294d1e5f5f9facf5"
  "8c750b088c7cd857d079c0eeb495da199b359461"
  "Apache-2.0"
  "MIT"
  "data/Timer.kt"
  "kernel/timer.c"
  "timers.c"
  "esp_timer_get_time()"
  "PicoTimer"
  "Apple Timer"
  "不直接采用 RTOS timer callback"
  "One-shot 语义优于周期 tick")
foreach(evidence IN LISTS required_evidence)
  string(FIND "${research_text}" "${evidence}" evidence_position)
  if(evidence_position EQUAL -1)
    message(FATAL_ERROR "Activation Timer research is missing: ${evidence}")
  endif()
endforeach()

file(GLOB build_manifests
  "${PROJECT_ROOT}/CMakeLists.txt"
  "${PROJECT_ROOT}/cmake/*.cmake"
  "${PROJECT_ROOT}/platformio.ini")
foreach(manifest IN LISTS build_manifests)
  if(manifest MATCHES "/audit_.*research[.]cmake$")
    continue()
  endif()
  file(READ "${manifest}" manifest_text)
  if(manifest_text MATCHES "cadenza-timer-research|deskclock|zephyr/kernel/timer|FreeRTOS-Kernel/timers")
    message(FATAL_ERROR "Read-only Timer research leaked into build: ${manifest}")
  endif()
endforeach()

file(GLOB_RECURSE app_sources
  "${PROJECT_ROOT}/lib/cadenza_apps/include/*.h"
  "${PROJECT_ROOT}/lib/cadenza_apps/src/*.cpp")
foreach(app_source IN LISTS app_sources)
  file(READ "${app_source}" source_text)
  if(source_text MATCHES "(esp_timer|xTimer|k_timer_|millis|micros)[ 	]*\\(")
    message(FATAL_ERROR "Platform/RTOS timer API leaked into App: ${app_source}")
  endif()
endforeach()

set(candidate_manifest
  "${PROJECT_ROOT}/.research/spikes/uac_idf/src/CMakeLists.txt")
file(READ "${candidate_manifest}" candidate_manifest_text)
string(FIND "${candidate_manifest_text}" "timer_service.cpp"
  candidate_timer_position)
if(candidate_timer_position EQUAL -1)
  message(FATAL_ERROR
    "ESP-IDF candidate source graph does not compile TimerService")
endif()

file(READ "${PROJECT_ROOT}/src/main.cpp" firmware_main_text)
if(NOT firmware_main_text MATCHES "esp_timer_get_time[ 	]*\\(")
  message(FATAL_ERROR "Firmware does not inject ESP monotonic time")
endif()

file(READ "${PROJECT_ROOT}/desktop/main.cpp" desktop_main_text)
if(NOT desktop_main_text MATCHES "SDL_GetTicks[ 	]*\\(")
  message(FATAL_ERROR "Desktop does not inject SDL monotonic time")
endif()
