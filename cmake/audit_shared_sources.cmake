if(NOT DEFINED PROJECT_ROOT)
  message(FATAL_ERROR "PROJECT_ROOT is required")
endif()

include("${PROJECT_ROOT}/cmake/cadenza_core_sources.cmake")
file(GLOB discovered RELATIVE "${PROJECT_ROOT}"
  "${PROJECT_ROOT}/lib/cadenza_core/src/*.cpp")
list(SORT discovered)
list(SORT CADENZA_CORE_SOURCES)
if(NOT discovered STREQUAL CADENZA_CORE_SOURCES)
  message(FATAL_ERROR
    "CMake core list does not match PlatformIO-discoverable sources.\n"
    "CMake: ${CADENZA_CORE_SOURCES}\nPlatformIO: ${discovered}")
endif()

foreach(duplicate IN ITEMS src/app_runtime.cpp src/apps.cpp src/mono_canvas.cpp)
  if(EXISTS "${PROJECT_ROOT}/${duplicate}")
    message(FATAL_ERROR "Superseded alternate implementation exists: ${duplicate}")
  endif()
endforeach()
