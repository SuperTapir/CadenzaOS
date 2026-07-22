#!/usr/bin/env bash
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1

BASE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BASELINE="$BASE_DIR/Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.kicad_sch"
EVIDENCE_DIR="$BASE_DIR/generated/evidence"
CANDIDATE_DIR="$BASE_DIR/generated/candidate"
CANDIDATE="$CANDIDATE_DIR/Cadenza-L1-Screen-Only.kicad_sch"
SUBSHEET="$CANDIDATE_DIR/Cadenza-L1-Screen-Only-Display.kicad_sch"
DONOR="$EVIDENCE_DIR/screen-only-donor.kicad_sch"
KICAD_CLI="/Applications/KiCad.app/Contents/MacOS/kicad-cli"
PYTHON_BIN="${CADENZA_PYTHON:-/Users/tapir/.local/share/uv/python/cpython-3.12.11-macos-aarch64-none/bin/python3.12}"
KIUTILS_PATH="/Users/tapir/Library/Python/3.13/lib/python/site-packages"
DISPLAY_FPC_LIBRARY="$BASE_DIR/../../libraries/display-fpc-variants"

mkdir -p "$EVIDENCE_DIR" "$CANDIDATE_DIR"
BASELINE_HASH_BEFORE="$(shasum -a 256 "$BASELINE" | awk '{print $1}')"

# Make the generated candidate independently openable by KiCad.  Without
# project-local library tables, command-line ERC reports hundreds of library
# lookup warnings that are unrelated to the screen-only electrical delta.
cp "$BASE_DIR/tools/candidate-support/sym-lib-table" "$CANDIDATE_DIR/sym-lib-table"
cp "$BASE_DIR/tools/candidate-support/fp-lib-table" "$CANDIDATE_DIR/fp-lib-table"
cp "$BASE_DIR/Cadenza-re-easyedapro.kicad_sym" "$CANDIDATE_DIR/Cadenza-re-easyedapro.kicad_sym"
rm -rf "$CANDIDATE_DIR/Cadenza-re-easyedapro.pretty"
cp -R "$BASE_DIR/Cadenza-re-easyedapro.pretty" "$CANDIDATE_DIR/Cadenza-re-easyedapro.pretty"
rm -rf "$CANDIDATE_DIR/Cadenza_Display_FPC_Variants.pretty"
cp -R "$DISPLAY_FPC_LIBRARY/Cadenza_Display_FPC_Variants.pretty" \
  "$CANDIDATE_DIR/Cadenza_Display_FPC_Variants.pretty"
cp "$BASE_DIR/Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.kicad_pro" \
  "$CANDIDATE_DIR/Cadenza-L1-Screen-Only.kicad_pro"

PYTHONPATH="$KIUTILS_PATH${PYTHONPATH:+:$PYTHONPATH}" "$PYTHON_BIN" \
  "$BASE_DIR/tools/generate_screen_only_donor.py" --output "$DONOR"
PYTHONPATH="$KIUTILS_PATH${PYTHONPATH:+:$PYTHONPATH}" "$PYTHON_BIN" \
  "$BASE_DIR/tools/transplant_screen_only_delta.py" \
  --source "$BASELINE" \
  --donor "$DONOR" \
  --output "$CANDIDATE" \
  --subsheet-output "$SUBSHEET"

"$KICAD_CLI" sch export netlist --format kicadxml \
  --output "$EVIDENCE_DIR/baseline.netlist.xml" "$BASELINE"
"$KICAD_CLI" sch export netlist --format kicadxml \
  --output "$EVIDENCE_DIR/candidate.netlist.xml" "$CANDIDATE"

"$KICAD_CLI" sch erc --format json --severity-all \
  --output "$EVIDENCE_DIR/baseline.erc.json" "$BASELINE"
"$KICAD_CLI" sch erc --format json --severity-all \
  --output "$EVIDENCE_DIR/candidate-self-contained.erc.json" "$CANDIDATE"

PYTHONPATH="$BASE_DIR/tools" "$PYTHON_BIN" "$BASE_DIR/tools/verify_screen_only_candidate.py" \
  --baseline-schematic "$BASELINE" \
  --candidate-schematic "$CANDIDATE" \
  --baseline-netlist "$EVIDENCE_DIR/baseline.netlist.xml" \
  --candidate-netlist "$EVIDENCE_DIR/candidate.netlist.xml" \
  --output-json "$EVIDENCE_DIR/verification-report.json" \
  --output-md "$EVIDENCE_DIR/VERIFICATION_REPORT.md"

"$PYTHON_BIN" "$BASE_DIR/../../gates/verify_l1_erc_delta.py" \
  --baseline "$EVIDENCE_DIR/baseline.erc.json" \
  --candidate "$EVIDENCE_DIR/candidate-self-contained.erc.json" \
  --output "$EVIDENCE_DIR/erc-delta.json"

BASELINE_HASH_AFTER="$(shasum -a 256 "$BASELINE" | awk '{print $1}')"
if [[ "$BASELINE_HASH_BEFORE" != "$BASELINE_HASH_AFTER" ]]; then
  echo "baseline changed during generation" >&2
  exit 1
fi
printf 'baseline_sha256_before=%s\nbaseline_sha256_after=%s\n' \
  "$BASELINE_HASH_BEFORE" "$BASELINE_HASH_AFTER"
