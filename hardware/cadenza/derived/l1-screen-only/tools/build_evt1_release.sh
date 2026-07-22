#!/usr/bin/env bash
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1

BASE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
PROVISIONAL_DIR="$BASE_DIR/generated/manufacturing-provisional"
RELEASE_DIR="$BASE_DIR/generated/release-l1-evt1-20260722"
PCB_DIR="$RELEASE_DIR/01-PCB"
PCBA_DIR="$RELEASE_DIR/02-PCBA"
PRINT_DIR="$RELEASE_DIR/03-3D-PRINT"
SOURCE_DIR="$RELEASE_DIR/04-SOURCE"
PRINT_PACKAGE="$BASE_DIR/../../mechanical/l1-fit-check/generated/Cadenza-L1-3D-print-package.zip"
CANDIDATE_DIR="$BASE_DIR/generated/candidate"

"$BASE_DIR/tools/build_provisional_manufacturing_bundle.sh"

mkdir -p "$PCB_DIR" "$PCBA_DIR" "$PRINT_DIR" "$SOURCE_DIR"
find "$RELEASE_DIR" -maxdepth 2 -type f -delete

cp "$PROVISIONAL_DIR/Cadenza-L1-Gerber.zip" "$PCB_DIR/"
cp "$PROVISIONAL_DIR/drill-report.txt" "$PCB_DIR/"
cp "$PROVISIONAL_DIR/Cadenza-L1-JLCPCB-BOM.csv" "$PCBA_DIR/"
cp "$PROVISIONAL_DIR/Cadenza-L1-JLCPCB-CPL.csv" "$PCBA_DIR/"
cp "$PROVISIONAL_DIR/manufacturing-bom-cpl-audit.json" "$PCBA_DIR/"
cp "$PROVISIONAL_DIR/jlc-provisional-dfm-audit.json" "$PCBA_DIR/"
cp "$PRINT_PACKAGE" "$PRINT_DIR/"
cp "$BASE_DIR/L1_EVT1_RELEASE.md" "$RELEASE_DIR/READ_ME_FIRST.md"

(cd "$CANDIDATE_DIR" && zip -q -FS "$SOURCE_DIR/Cadenza-L1-KiCad-source.zip" \
  Cadenza-L1-Screen-Only.kicad_pcb \
  Cadenza-L1-Screen-Only.kicad_sch \
  Cadenza-L1-Screen-Only.kicad_pro)

(cd "$RELEASE_DIR" && find . -type f ! -name SHA256SUMS -print0 | sort -z | \
  xargs -0 shasum -a 256 > SHA256SUMS)

rm -f "$BASE_DIR/generated/Cadenza-L1-EVT1-release-20260722.zip"
(cd "$RELEASE_DIR" && zip -q -r "$BASE_DIR/generated/Cadenza-L1-EVT1-release-20260722.zip" .)

printf 'EVT1 release: %s\n' "$BASE_DIR/generated/Cadenza-L1-EVT1-release-20260722.zip"
