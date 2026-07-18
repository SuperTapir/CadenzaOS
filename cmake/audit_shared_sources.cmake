if(NOT DEFINED PROJECT_ROOT)
  message(FATAL_ERROR "PROJECT_ROOT is required")
endif()

include("${PROJECT_ROOT}/cmake/cadenza_core_sources.cmake")
include("${PROJECT_ROOT}/cmake/cadenza_apps_sources.cmake")
file(GLOB discovered RELATIVE "${PROJECT_ROOT}"
  "${PROJECT_ROOT}/lib/cadenza_core/src/*.cpp")
list(SORT discovered)
list(SORT CADENZA_CORE_SOURCES)
if(NOT discovered STREQUAL CADENZA_CORE_SOURCES)
  message(FATAL_ERROR
    "CMake core list does not match PlatformIO-discoverable sources.\n"
    "CMake: ${CADENZA_CORE_SOURCES}\nPlatformIO: ${discovered}")
endif()

file(GLOB discovered_apps RELATIVE "${PROJECT_ROOT}"
  "${PROJECT_ROOT}/lib/cadenza_apps/src/*.cpp")
list(SORT discovered_apps)
list(SORT CADENZA_APPS_SOURCES)
if(NOT discovered_apps STREQUAL CADENZA_APPS_SOURCES)
  message(FATAL_ERROR
    "CMake Apps list does not match PlatformIO-discoverable sources.\n"
    "CMake: ${CADENZA_APPS_SOURCES}\nPlatformIO: ${discovered_apps}")
endif()

foreach(duplicate IN ITEMS src/app_runtime.cpp src/apps.cpp src/mono_canvas.cpp)
  if(EXISTS "${PROJECT_ROOT}/${duplicate}")
    message(FATAL_ERROR "Superseded alternate implementation exists: ${duplicate}")
  endif()
endforeach()

file(GLOB_RECURSE portable_files
  "${PROJECT_ROOT}/lib/cadenza_core/include/*.h"
  "${PROJECT_ROOT}/lib/cadenza_core/src/*.cpp"
  "${PROJECT_ROOT}/lib/cadenza_host/include/*.h"
  "${PROJECT_ROOT}/lib/cadenza_host/src/*.cpp"
  "${PROJECT_ROOT}/lib/cadenza_apps/include/*.h"
  "${PROJECT_ROOT}/lib/cadenza_apps/src/*.cpp"
  "${PROJECT_ROOT}/lib/cadenza_system/include/*.h"
  "${PROJECT_ROOT}/lib/cadenza_system/src/*.cpp")
foreach(portable_file IN LISTS portable_files)
  file(READ "${portable_file}" portable_source)
  if(portable_source MATCHES "#[ \t]*include[ \t]*[<\"](Arduino|TFT_eSPI|SDL|esp_|nvs|nimble|host/ble|sys/socket|lwip|WiFi)")
    message(FATAL_ERROR "Platform header leaked into ${portable_file}")
  endif()
  if(portable_source MATCHES "(Serial|millis|micros|digitalRead|digitalWrite|pinMode)[ \t]*\\(")
    message(FATAL_ERROR "Platform API leaked into ${portable_file}")
  endif()
  if(portable_source MATCHES "(esp_wifi_|nvs_|ble_gap_|wifi_prov_|socket)[A-Za-z0-9_]*[ \t]*\\(")
    message(FATAL_ERROR "Connectivity platform API leaked into ${portable_file}")
  endif()
endforeach()

file(GLOB_RECURSE core_files
  "${PROJECT_ROOT}/lib/cadenza_core/include/*.h"
  "${PROJECT_ROOT}/lib/cadenza_core/src/*.cpp")
foreach(core_file IN LISTS core_files)
  file(READ "${core_file}" core_source)
  if(core_source MATCHES "(LauncherApp|ClockApp|MotionApp|SettingsApp|AnimationGalleryApp|kLauncherAppId|kGalleryAppId)")
    message(FATAL_ERROR "Bundled App leaked into core: ${core_file}")
  endif()
endforeach()

file(GLOB_RECURSE bundled_app_files
  "${PROJECT_ROOT}/lib/cadenza_apps/include/*.h"
  "${PROJECT_ROOT}/lib/cadenza_apps/src/*.cpp")
foreach(app_file IN LISTS bundled_app_files)
  file(READ "${app_file}" app_source)
  if(app_source MATCHES "AppRuntime|cadenza/core/app_runtime.h")
    message(FATAL_ERROR "Bundled App can access full Runtime: ${app_file}")
  endif()
  if(app_source MATCHES "ResourceOwner|SystemOperationEnvelope|AppCommandPort")
    message(FATAL_ERROR
      "Bundled App attempted to construct caller identity or command port: ${app_file}")
  endif()
endforeach()
