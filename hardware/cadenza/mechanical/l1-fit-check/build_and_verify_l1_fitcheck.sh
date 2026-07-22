#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../../../.." && pwd)
KICAD_PY=/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3
CAD_PY=/opt/homebrew/bin/python3.13
CAD_PATH=/Users/tapir/.cache/cadenza-cad-tools-py313

cd "$ROOT"
"$KICAD_PY" hardware/cadenza/mechanical/l1-fit-check/extract_candidate_pcb_mechanical.py
PYTHONPATH="$CAD_PATH" "$CAD_PY" hardware/cadenza/mechanical/l1-fit-check/generate_l1_front_fitcheck.py
PYTHONPATH="$CAD_PATH" "$CAD_PY" hardware/cadenza/mechanical/l1-fit-check/render_l1_fitcheck.py
PYTHONPATH="$CAD_PATH" "$CAD_PY" hardware/cadenza/mechanical/l1-fit-check/audit_l1_fitcheck.py
PYTHONPATH="$CAD_PATH" "$CAD_PY" hardware/cadenza/mechanical/l1-fit-check/generate_l1_risk_register.py
PYTHONPATH="$CAD_PATH" "$CAD_PY" hardware/cadenza/mechanical/l1-fit-check/build_l1_print_package.py
