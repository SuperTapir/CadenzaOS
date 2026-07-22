#!/usr/bin/env bash
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1

BASE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
CANDIDATE_DIR="$BASE_DIR/generated/candidate"
OUTPUT_DIR="$BASE_DIR/generated/manufacturing-provisional"
GERBER_DIR="$OUTPUT_DIR/gerbers"
BOARD="$CANDIDATE_DIR/Cadenza-L1-Screen-Only.kicad_pcb"
SCHEMATIC="$CANDIDATE_DIR/Cadenza-L1-Screen-Only.kicad_sch"
KICAD_CLI="/Applications/KiCad.app/Contents/MacOS/kicad-cli"
PYTHON_BIN="${CADENZA_PYTHON:-/Users/tapir/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3}"

mkdir -p "$GERBER_DIR"
find "$GERBER_DIR" -maxdepth 1 -type f -delete

"$KICAD_CLI" pcb export gerbers \
  --layers 'F.Cu,B.Cu,F.Paste,B.Paste,F.Silkscreen,B.Silkscreen,F.Mask,B.Mask,Edge.Cuts' \
  --subtract-soldermask --precision 6 --output "$GERBER_DIR" "$BOARD"
"$KICAD_CLI" pcb export drill --format excellon --excellon-units mm \
  --excellon-separate-th --generate-map --map-format svg \
  --generate-report --report-path "$OUTPUT_DIR/drill-report.txt" \
  --output "$GERBER_DIR" "$BOARD"

"$KICAD_CLI" sch export bom \
  --fields 'Reference,Value,Footprint,Supplier Part,Manufacturer Part,Manufacturer,QUANTITY' \
  --labels 'Designator,Comment,Footprint,LCSC Part #,Manufacturer Part,Manufacturer,Quantity' \
  --output "$OUTPUT_DIR/bom-raw.csv" "$SCHEMATIC"
"$KICAD_CLI" pcb export pos --side both --format csv --units mm --exclude-dnp \
  --output "$OUTPUT_DIR/positions-raw.csv" "$BOARD"

"$PYTHON_BIN" "$BASE_DIR/tools/make_jlc_bom_cpl.py" \
  --bom-raw "$OUTPUT_DIR/bom-raw.csv" \
  --pos-raw "$OUTPUT_DIR/positions-raw.csv" \
  --bom-output "$OUTPUT_DIR/Cadenza-L1-JLCPCB-BOM.csv" \
  --cpl-output "$OUTPUT_DIR/Cadenza-L1-JLCPCB-CPL.csv" \
  --audit-output "$OUTPUT_DIR/manufacturing-bom-cpl-audit.json"

"$KICAD_CLI" pcb drc --format json --severity-all \
  --output "$OUTPUT_DIR/candidate-pcb.drc.json" "$BOARD" || test -s "$OUTPUT_DIR/candidate-pcb.drc.json"
"/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3" \
  "$BASE_DIR/tools/verify_jlc_dfm.py" \
  --board "$BOARD" \
  --drc "$OUTPUT_DIR/candidate-pcb.drc.json" \
  --bundle-dir "$OUTPUT_DIR" \
  --output "$OUTPUT_DIR/jlc-provisional-dfm-audit.json"

(cd "$GERBER_DIR" && zip -q -FS "$OUTPUT_DIR/Cadenza-L1-Gerber.zip" ./*)
(cd "$OUTPUT_DIR" && shasum -a 256 Cadenza-L1-Gerber.zip Cadenza-L1-JLCPCB-BOM.csv \
  Cadenza-L1-JLCPCB-CPL.csv > SHA256SUMS)

printf 'Provisional manufacturing bundle: %s\n' "$OUTPUT_DIR"
