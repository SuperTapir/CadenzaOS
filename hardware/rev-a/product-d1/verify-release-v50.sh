#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
KICAD="$ROOT/kicad"
PROJECT="$KICAD/production-v42-priority/cadenza-d1-rev-a-production.kicad_pro"
PCB="$KICAD/production-v42-priority/cadenza-d1-rev-a-production.kicad_pcb"
SCH="$KICAD/cadenza-d1.kicad_sch"
RELEASE="$KICAD/production-v50-release-20260721"
ASSEMBLY="$RELEASE/assembly"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

pass() { printf 'PASS  %s\n' "$1"; }
fail() { printf 'FAIL  %s\n' "$1" >&2; exit 1; }
need_file() { test -s "$1" || fail "missing or empty: $1"; }

command -v kicad-cli >/dev/null || fail "kicad-cli is not installed"
command -v jq >/dev/null || fail "jq is not installed"
command -v unzip >/dev/null || fail "unzip is not installed"

need_file "$PROJECT"
need_file "$PCB"
need_file "$SCH"
need_file "$ROOT/RELEASE-DRAFT-v50.md"
need_file "$ROOT/net-contract.json"
need_file "$ROOT/circuit.json"
need_file "$ROOT/mechanical/cadenza-d1-enclosure.scad"
pass "authoritative source files exist"

jq -e '.gpio.SPK_BCLK == 16 and .gpio.SD_DET == 9' \
  "$ROOT/net-contract.json" >/dev/null || fail "GPIO contract does not match v50"
jq -e '.board.thickness_mm == 1.6 and .board.outline_mm.width == 116 and .board.outline_mm.height == 64' \
  "$ROOT/circuit.json" >/dev/null || fail "board mechanical contract does not match v50"
pass "JSON contracts match v50"

kicad-cli pcb drc --exit-code-violations --output "$TMP/drc.rpt" "$PCB" >/dev/null
kicad-cli sch erc --exit-code-violations --output "$TMP/erc.rpt" "$SCH" >/dev/null
pass "KiCad DRC and ERC have zero violations"

for artifact in \
  "$RELEASE/cadenza-d1-v50-gerbers.zip" \
  "$RELEASE/cadenza-d1-v50-drill.zip" \
  "$RELEASE/cadenza-d1-v50-kicad-import.zip" \
  "$RELEASE/cadenza-d1-v50.ipc" \
  "$RELEASE/mechanical/cadenza-d1-v50.step" \
  "$ASSEMBLY/cadenza-d1-v50-jlc-core-bom.csv" \
  "$ASSEMBLY/cadenza-d1-v50-jlc-core-cpl.csv" \
  "$ASSEMBLY/cadenza-d1-v50-hand-frozen.csv" \
  "$ASSEMBLY/cadenza-d1-v50-hand-tbd.csv"; do
  need_file "$artifact"
done

MODEL_DIR="$ROOT/kicad/production-v42-priority/cadenza-d1.3dshapes"
MODEL_PCB="$ROOT/kicad/production-v42-priority/cadenza-d1-rev-a-production.kicad_pcb"
for model in \
  SW1_C221660_JS102011SAQN.step \
  J1_C2997435_USB-C.step \
  J4_J5_C160352_JST-PH.step \
  U2_C54313_BQ24074.step \
  U5_C910544_MAX98357.step; do
  need_file "$MODEL_DIR/$model"
  grep -Fq "\${KIPRJMOD}/cadenza-d1.3dshapes/$model" "$MODEL_PCB" || fail "PCB model binding missing: $model"
done
test "$(grep -Fc '${KIPRJMOD}/cadenza-d1.3dshapes/J4_J5_C160352_JST-PH.step' "$MODEL_PCB")" -eq 2 || fail "shared J4/J5 model must be bound exactly twice"
pass "five local STEP files cover six critical mechanical references"
pass "release artifacts exist"

GERBER_LIST="$(unzip -Z1 "$RELEASE/cadenza-d1-v50-gerbers.zip")"
for suffix in F_Cu.gtl B_Cu.gbl GND.g1 POWER.g2 F_Mask.gts B_Mask.gbs F_Silkscreen.gto B_Silkscreen.gbo Edge_Cuts.gm1; do
  grep -Fq "$suffix" <<<"$GERBER_LIST" || fail "Gerber layer missing: $suffix"
done
DRILL_LIST="$(unzip -Z1 "$RELEASE/cadenza-d1-v50-drill.zip")"
grep -Eq '\.(drl|xln)$' <<<"$DRILL_LIST" || fail "drill archive has no drill file"
pass "Gerber and drill archives contain required layers"

IMPORT_LIST="$(unzip -Z1 "$RELEASE/cadenza-d1-v50-kicad-import.zip")"
grep -Fq '.kicad_pcb' <<<"$IMPORT_LIST" || fail "import ZIP missing PCB"
grep -Fq '.kicad_sch' <<<"$IMPORT_LIST" || fail "import ZIP missing schematic"
grep -Fq '.kicad_pro' <<<"$IMPORT_LIST" || fail "import ZIP missing project"
grep -Fq 'cadenza-d1.3dshapes/' <<<"$IMPORT_LIST" || fail "import ZIP missing local 3D models"
pass "JLCEDA KiCad import archive is complete"

python3 - "$ASSEMBLY/cadenza-d1-v50-jlc-core-bom.csv" "$ASSEMBLY/cadenza-d1-v50-jlc-core-cpl.csv" <<'PY'
import csv
import sys

def refs(path):
    result = set()
    with open(path, newline='', encoding='utf-8-sig') as handle:
        for row in csv.DictReader(handle):
            raw = row.get('Designator') or row.get('Designators') or row.get('Ref') or row.get('Reference') or ''
            result.update(item.strip() for item in raw.replace(';', ',').split(',') if item.strip())
    return result

bom, cpl = refs(sys.argv[1]), refs(sys.argv[2])
if bom != cpl:
    print('BOM-only:', sorted(bom - cpl), file=sys.stderr)
    print('CPL-only:', sorted(cpl - bom), file=sys.stderr)
    raise SystemExit(1)
if len(bom) != 60:
    print(f'expected 60 core placements, got {len(bom)}', file=sys.stderr)
    raise SystemExit(1)
PY
pass "core BOM/CPL parity is exactly 60 placements"

TBD_ROWS="$(python3 - "$ASSEMBLY/cadenza-d1-v50-hand-tbd.csv" <<'PY'
import csv, sys
with open(sys.argv[1], newline='', encoding='utf-8-sig') as f:
    print(sum(1 for _ in csv.DictReader(f)))
PY
)"
test "$TBD_ROWS" -eq 0 || fail "expected 0 grouped hand/TBD rows, got $TBD_ROWS"
pass "all hand-assembled electrical parts have frozen MPN and LCSC IDs"

if command -v openscad >/dev/null; then
  openscad -o "$TMP/cadenza-d1-enclosure-v50.stl" "$ROOT/mechanical/cadenza-d1-enclosure.scad" >/dev/null
  need_file "$TMP/cadenza-d1-enclosure-v50.stl"
  pass "OpenSCAD enclosure compiles"
  "$ROOT/mechanical/check-collision-v50.sh"
else
  printf 'SKIP  OpenSCAD enclosure compile (openscad not installed)\n'
fi

cat <<'EOF'

LOCAL RELEASE GATE PASSED

Still requires human/physical evidence before ordering:
  1. JLCEDA Pro v50 import visual review and JLCEDA DRC.
  2. Exact LCD FPC pinout/contact/mechanical confirmation.
  3. Sample-fit hand-frozen mechanical and high-current parts before batch SMT transfer.
  4. Run physical enclosure fit-check with measured LCD, battery, speaker and keycaps.
EOF
