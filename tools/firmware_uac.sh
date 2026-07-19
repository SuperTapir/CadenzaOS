#!/usr/bin/env bash
# Primary ESP-IDF 5.5 T-Embed firmware entry for CadenzaOS (Runtime + UAC).
# PlatformIO rollback: tools/check.sh firmware-pio
set -euo pipefail

project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
spike_dir="${project_root}/.research/spikes/uac_idf"
build_dir="${spike_dir}/build-idf-hardware"
idf_path="${CADENZA_IDF_PATH:-${IDF_PATH:-${project_root}/../esp-idf-v5.5}}"
mode="${1:-build}"

usage() {
  cat <<'EOF'
usage: tools/firmware_uac.sh <build|size|flash|connectivity>

  build         Build Runtime + ES7210 + UAC2/CDC hardware firmware (default)
  size          Print size report for the hardware build
  flash         Flash build-idf-hardware to a connected T-Embed
  connectivity  Build the Wi-Fi/NimBLE/Security 2 candidate (separate dir)

Environment:
  CADENZA_IDF_PATH   ESP-IDF v5.5 checkout (default: ../esp-idf-v5.5 or $IDF_PATH)
  CADENZA_UAC_PORT   Serial port for flash (default: first /dev/cu.usbmodem*)
EOF
}

require_idf() {
  if [[ ! -f "${idf_path}/export.sh" ]]; then
    echo "ESP-IDF v5.5 not found at: ${idf_path}" >&2
    echo "Set CADENZA_IDF_PATH to the official checkout used by CadenzaOS." >&2
    exit 1
  fi
  # shellcheck disable=SC1091
  source "${idf_path}/export.sh" >/dev/null
}

build_hardware() {
  require_idf
  cd "${spike_dir}"
  idf.py -B build-idf-hardware \
    -DSDKCONFIG="${spike_dir}/sdkconfig.hardware" \
    -DSDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.hardware.defaults' \
    build
}

build_connectivity() {
  require_idf
  cd "${spike_dir}"
  idf.py -B build-idf-connectivity \
    -DSDKCONFIG="${spike_dir}/sdkconfig.connectivity" \
    -DSDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.hardware.defaults' \
    build
}

size_hardware() {
  require_idf
  cd "${spike_dir}"
  if [[ ! -f "${build_dir}/cadenza_uac_spike.bin" ]]; then
    build_hardware
  fi
  idf.py -B build-idf-hardware size
}

flash_hardware() {
  require_idf
  if [[ ! -f "${build_dir}/cadenza_uac_spike.bin" ]]; then
    build_hardware
  fi

  local port="${CADENZA_UAC_PORT:-}"
  if [[ -z "${port}" ]]; then
    port="$(ls /dev/cu.usbmodem* 2>/dev/null | head -1 || true)"
  fi
  if [[ -z "${port}" ]]; then
    cat <<'EOF' >&2
No /dev/cu.usbmodem* port found.

Enter T-Embed download mode, then retry:
  1. Hold BOOT (encoder center)
  2. Tap RST on the back, release RST
  3. Release BOOT
  4. tools/firmware_uac.sh flash

Or set CADENZA_UAC_PORT explicitly.
EOF
    exit 1
  fi

  echo "Flashing ${build_dir}/cadenza_uac_spike.bin via ${port}"
  python -m esptool --chip esp32s3 -p "${port}" -b 460800 \
    --before default_reset --after hard_reset write_flash \
    --flash_mode dio --flash_freq 80m --flash_size 2MB \
    0x0 "${build_dir}/bootloader/bootloader.bin" \
    0x8000 "${build_dir}/partition_table/partition-table.bin" \
    0x10000 "${build_dir}/cadenza_uac_spike.bin"
}

case "${mode}" in
  -h|--help|help)
    usage
    ;;
  build)
    build_hardware
    ;;
  size)
    size_hardware
    ;;
  flash)
    flash_hardware
    ;;
  connectivity)
    build_connectivity
    ;;
  *)
    echo "unknown mode: ${mode}" >&2
    usage >&2
    exit 2
    ;;
esac
