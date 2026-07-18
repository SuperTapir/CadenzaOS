#!/usr/bin/env bash
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${project_root}/build/host"
pio_command="${CADENZA_PIO:-${project_root}/../.platformio-env/bin/pio}"
mode="${1:-host}"

configure_host() {
  cmake --preset host-debug -S "${project_root}"
}

build_host() {
  configure_host
  cmake --build "${build_dir}" --parallel
}

run_host_tests() {
  build_host
  ctest --test-dir "${build_dir}" --output-on-failure
}

case "${mode}" in
  focused)
    test_pattern="${2:?usage: tools/check.sh focused <ctest-regex>}"
    build_host
    ctest --test-dir "${build_dir}" --output-on-failure -R "${test_pattern}"
    ;;
  host)
    run_host_tests
    ;;
  desktop)
    configure_host
    cmake --build "${build_dir}" --target cadenza_desktop --parallel
    "${build_dir}/cadenza_desktop"
    ;;
  firmware)
    "${pio_command}" run --project-dir "${project_root}"
    ;;
  diff)
    git -C "${project_root}" diff --check
    ;;
  all)
    run_host_tests
    "${build_dir}/cadenza_desktop"
    "${pio_command}" run --project-dir "${project_root}"
    git -C "${project_root}" diff --check
    ;;
  *)
    echo "unknown check mode: ${mode}" >&2
    echo "modes: focused, host, desktop, firmware, diff, all" >&2
    exit 2
    ;;
esac
