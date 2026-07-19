if(NOT DEFINED PROJECT_ROOT)
  message(FATAL_ERROR "PROJECT_ROOT is required")
endif()

set(mapping "${PROJECT_ROOT}/lib/cadenza_core/src/system_typography.cpp")
file(READ "${mapping}" mapping_source)
if(mapping_source MATCHES "FramebufferProfile|framebufferProfile|displayProfile")
  message(FATAL_ERROR
    "system typography role mapping must not read a display profile")
endif()

set(product_sources
  "lib/cadenza_core/src/app_runtime.cpp"
  "lib/cadenza_core/src/system_surface.cpp"
  "lib/cadenza_apps/src/apps.cpp"
  "lib/cadenza_apps/src/gallery.cpp"
  "lib/cadenza_system/include/cadenza/system/frame_coordinator.h"
)

foreach(relative_path IN LISTS product_sources)
  file(READ "${PROJECT_ROOT}/${relative_path}" source)
  if(source MATCHES
      "TypographyDensity|typographyDensityForViewport|resolveTypography|kRoobert")
    message(FATAL_ERROR
      "${relative_path} must select semantic roles, not viewport/device font resources")
  endif()
  if(source MATCHES "preferredScale|minimumScale")
    message(FATAL_ERROR
      "${relative_path} must use semantic native-size roles, not scale fallback")
  endif()
  string(REGEX REPLACE
    "\\.text\\([^;]*TextRole::[^;]*\\);"
    "" calls_without_explicit_role "${source}")
  if(calls_without_explicit_role MATCHES "\\.text\\(")
    message(FATAL_ERROR
      "${relative_path} contains a product text call without an explicit TextRole")
  endif()
  if(source MATCHES
      "\\.text\\([^;]*,[ \t\r\n]*[2-9][0-9]*[ \t\r\n]*,[ \t\r\n]*(true|false)")
    message(FATAL_ERROR
      "${relative_path} contains a product text call scaled above native size")
  endif()
endforeach()

message(STATUS "System typography source audit passed")
