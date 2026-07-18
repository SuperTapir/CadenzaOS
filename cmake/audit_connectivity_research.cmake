if(NOT DEFINED PROJECT_ROOT)
  message(FATAL_ERROR "PROJECT_ROOT is required")
endif()

set(research_doc
  "${PROJECT_ROOT}/docs/connectivity-capability-reference-research.md")
if(NOT EXISTS "${research_doc}")
  message(FATAL_ERROR "Missing connectivity research record: ${research_doc}")
endif()
file(READ "${research_doc}" research_text)

set(required_evidence
  "8c750b088c7cd857d079c0eeb495da199b359461"
  "https://github.com/espressif/esp-idf/tree/8c750b088c7cd857d079c0eeb495da199b359461"
  "components/wifi_provisioning/src/manager.c"
  "684c9e8f32e4373a21098559f748f06915f950c9"
  "https://github.com/zephyrproject-rtos/zephyr/tree/684c9e8f32e4373a21098559f748f06915f950c9"
  "subsys/net/conn_mgr/conn_mgr_monitor.c"
  "7a79a1d119f85e6fb5945cee27e341334f42e4e8"
  "https://github.com/tock/tock/tree/7a79a1d119f85e6fb5945cee27e341334f42e4e8"
  "kernel/src/capabilities.rs"
  "https://fuchsia.dev/fuchsia-src/development/components/connect"
  "Apache-2.0"
  "MIT")
foreach(evidence IN LISTS required_evidence)
  string(FIND "${research_text}" "${evidence}" evidence_position)
  if(evidence_position EQUAL -1)
    message(FATAL_ERROR "Connectivity research is missing evidence: ${evidence}")
  endif()
endforeach()

function(verify_optional_clone relative_path expected_commit)
  set(clone_root "${PROJECT_ROOT}/${relative_path}")
  if(NOT EXISTS "${clone_root}/.git")
    return()
  endif()
  execute_process(
    COMMAND git -C "${clone_root}" rev-parse HEAD
    OUTPUT_VARIABLE actual_commit
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE git_result)
  if(NOT git_result EQUAL 0 OR NOT actual_commit STREQUAL expected_commit)
    message(FATAL_ERROR
      "Research clone ${relative_path} is ${actual_commit}; expected ${expected_commit}")
  endif()
endfunction()

verify_optional_clone(
  ".research/references/system-zephyr"
  "684c9e8f32e4373a21098559f748f06915f950c9")
verify_optional_clone(
  ".research/references/system-tock"
  "7a79a1d119f85e6fb5945cee27e341334f42e4e8")

file(GLOB build_manifests
  "${PROJECT_ROOT}/CMakeLists.txt"
  "${PROJECT_ROOT}/cmake/*.cmake"
  "${PROJECT_ROOT}/platformio.ini"
  "${PROJECT_ROOT}/.research/spikes/uac_idf/CMakeLists.txt"
  "${PROJECT_ROOT}/.research/spikes/uac_idf/src/CMakeLists.txt")
foreach(manifest IN LISTS build_manifests)
  if(manifest MATCHES "/audit_.*research[.]cmake$")
    continue()
  endif()
  file(READ "${manifest}" manifest_text)
  if(manifest_text MATCHES "[.]research/references")
    message(FATAL_ERROR
      "Read-only research clone leaked into build manifest: ${manifest}")
  endif()
endforeach()

file(READ "${PROJECT_ROOT}/.gitignore" gitignore_text)
if(NOT gitignore_text MATCHES "[.]research/[*]")
  message(FATAL_ERROR "Research reference clones are not ignored")
endif()
