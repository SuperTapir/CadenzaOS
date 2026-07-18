set(required_files
  "THIRD_PARTY_NOTICES.md"
  "docs/visual-assets.md"
  "third_party/u8g2/licenses/LICENSE"
  "third_party/licenses/doctest-LICENSE.txt"
  "third_party/licenses/stb-LICENSE.txt"
  "third_party/licenses/gif-h-LICENSE.txt"
  "third_party/doctest/doctest.h"
  "third_party/stb/stb_image_write.h"
  "third_party/gif/gif.h"
)

foreach(relative_path IN LISTS required_files)
  if(NOT EXISTS "${PROJECT_ROOT}/${relative_path}")
    message(FATAL_ERROR "Missing vendored dependency or license: ${relative_path}")
  endif()
endforeach()

file(READ "${PROJECT_ROOT}/docs/visual-assets.md" asset_manifest)
foreach(asset IN ITEMS
    "u8g2_font_6x10_tf"
    "ordered 8×8 threshold table"
    "Gallery four-frame 2×4 sprite")
  string(FIND "${asset_manifest}" "${asset}" asset_position)
  if(asset_position EQUAL -1)
    message(FATAL_ERROR "Visual asset manifest is missing ${asset}")
  endif()
endforeach()

file(READ "${PROJECT_ROOT}/THIRD_PARTY_NOTICES.md" notices)
set(required_pins
  "ab9e48b2228351e9476682a70b7f3ee4909cd585"
  "d44d4f6e66232d716af82f00a063759e9d0e50d6"
  "31c1ad37456438565541f4919958214b6e762fb4"
  "70b645280d5e687f5217177c9cfa2889b0a2ad5f"
)

foreach(pin IN LISTS required_pins)
  string(FIND "${notices}" "${pin}" pin_position)
  if(pin_position EQUAL -1)
    message(FATAL_ERROR "THIRD_PARTY_NOTICES.md is missing pin ${pin}")
  endif()
endforeach()
