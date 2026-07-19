if(NOT DEFINED PROJECT_ROOT)
  message(FATAL_ERROR "PROJECT_ROOT is required")
endif()

file(GLOB_RECURSE current_product_files
  "${PROJECT_ROOT}/lib/*.h"
  "${PROJECT_ROOT}/lib/*.cpp"
  "${PROJECT_ROOT}/src/*.h"
  "${PROJECT_ROOT}/src/*.cpp"
  "${PROJECT_ROOT}/desktop/*.h"
  "${PROJECT_ROOT}/desktop/*.cpp"
  "${PROJECT_ROOT}/tests/*.h"
  "${PROJECT_ROOT}/tests/*.cpp"
  "${PROJECT_ROOT}/tools/*.h"
  "${PROJECT_ROOT}/tools/*.cpp"
  "${PROJECT_ROOT}/tools/*.py"
  "${PROJECT_ROOT}/.research/spikes/*.h"
  "${PROJECT_ROOT}/.research/spikes/*.cpp")

foreach(product_file IN LISTS current_product_files)
  file(READ "${product_file}" product_text)
  if(product_text MATCHES "ClockApp|kClockAppId")
    message(FATAL_ERROR "Legacy Timer C++ name remains in ${product_file}")
  endif()
endforeach()
set(id_header
  "${PROJECT_ROOT}/lib/cadenza_apps/include/cadenza/apps/builtin_app_ids.h")
file(READ "${id_header}" id_text)
if(NOT id_text MATCHES "kTimerAppId[^{]*[{][ ]*0x0101[ ]*[}]")
  message(FATAL_ERROR "kTimerAppId must preserve catalog value 0x0101")
endif()

set(legacy_cover_paths
  "${PROJECT_ROOT}/assets/launcher-covers/clock.pbm"
  "${PROJECT_ROOT}/assets/launcher-covers/clock_t_embed.pbm"
  "${PROJECT_ROOT}/assets/launcher-covers/source/clock.png"
  "${PROJECT_ROOT}/lib/cadenza_apps/src/generated/clock_cover.h"
  "${PROJECT_ROOT}/lib/cadenza_apps/src/generated/clock_t_embed_cover.h")
foreach(legacy_cover IN LISTS legacy_cover_paths)
  if(EXISTS "${legacy_cover}")
    message(FATAL_ERROR "Legacy Clock Cover remains: ${legacy_cover}")
  endif()
endforeach()
