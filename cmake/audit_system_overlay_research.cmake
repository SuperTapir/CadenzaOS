if(NOT DEFINED PROJECT_ROOT)
  message(FATAL_ERROR "PROJECT_ROOT is required")
endif()

set(research_doc
  "${PROJECT_ROOT}/docs/system-overlay-reference-research.md")
set(decision_doc
  "${PROJECT_ROOT}/docs/system-overlay-adoption-decision.md")
foreach(document IN ITEMS "${research_doc}" "${decision_doc}")
  if(NOT EXISTS "${document}")
    message(FATAL_ERROR "Missing System Overlay research artifact: ${document}")
  endif()
endforeach()

file(READ "${research_doc}" research_text)
file(READ "${decision_doc}" decision_text)
set(required_research_evidence
  "https://sdk.play.date/3.1.0/Inside%20Playdate.html"
  "https://sdk.play.date/changelog/"
  "c016f72d4c125098287be5e83c0f1abed4706ee5"
  "src/indev/lv_indev.c"
  "ad2a80042349a0cc6e0a14541e985d798a89f389"
  "applications/services/gui/gui.c"
  "a02bed441965ee1f18f856352c7d5ee5ba35d795"
  "qquickoverlay.cpp"
  "0850aba860959c3e75fb3e97120ca92957f9d057"
  "93ffd6eb29d61c1af1cb8d40b78daf75c4e85946"
  "GPL-3.0"
  "LGPL-3.0-only"
  "MIT")
foreach(evidence IN LISTS required_research_evidence)
  string(FIND "${research_text}" "${evidence}" evidence_position)
  if(evidence_position EQUAL -1)
    message(FATAL_ERROR "System Overlay research is missing evidence: ${evidence}")
  endif()
endforeach()

set(required_decisions
  "完整 button sequence owner"
  "Suspend-with-snapshot"
  "Transition defer"
  "Typed async action"
  "不引入 LVGL、Qt Quick、microui 或 Flipper GUI")
foreach(decision IN LISTS required_decisions)
  string(FIND "${decision_text}" "${decision}" decision_position)
  if(decision_position EQUAL -1)
    message(FATAL_ERROR "System Overlay adoption decision is missing: ${decision}")
  endif()
endforeach()

function(verify_optional_overlay_clone relative_path expected_commit)
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
      "Overlay research clone ${relative_path} is ${actual_commit}; expected ${expected_commit}")
  endif()
endfunction()

verify_optional_overlay_clone(
  ".research/references/overlay-lvgl"
  "c016f72d4c125098287be5e83c0f1abed4706ee5")
verify_optional_overlay_clone(
  ".research/references/overlay-flipper"
  "ad2a80042349a0cc6e0a14541e985d798a89f389")
verify_optional_overlay_clone(
  ".research/references/overlay-qtdeclarative"
  "a02bed441965ee1f18f856352c7d5ee5ba35d795")
verify_optional_overlay_clone(
  ".research/references/microui"
  "0850aba860959c3e75fb3e97120ca92957f9d057")
verify_optional_overlay_clone(
  ".research/references/NobleEngine"
  "93ffd6eb29d61c1af1cb8d40b78daf75c4e85946")

file(GLOB build_manifests
  "${PROJECT_ROOT}/CMakeLists.txt"
  "${PROJECT_ROOT}/cmake/*.cmake"
  "${PROJECT_ROOT}/platformio.ini")
foreach(manifest IN LISTS build_manifests)
  if(manifest MATCHES "/audit_.*research[.]cmake$")
    continue()
  endif()
  file(READ "${manifest}" manifest_text)
  if(manifest_text MATCHES "[.]research/references")
    message(FATAL_ERROR
      "Read-only overlay research clone leaked into build manifest: ${manifest}")
  endif()
endforeach()

file(READ "${PROJECT_ROOT}/.gitignore" gitignore_text)
if(NOT gitignore_text MATCHES "[.]research/[*]")
  message(FATAL_ERROR "Research reference clones are not ignored")
endif()
