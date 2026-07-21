#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
SCAD="$HERE/cadenza-d1-collision-review.scad"
MESH="$HERE/../kicad/production-v50-release-20260721/mechanical/cadenza-d1-v50-pcb-aligned.stl"
TMP="$(mktemp -d)"

test -s "$MESH" || { echo "FAIL  aligned PCB mesh missing" >&2; exit 1; }

set +e
openscad --backend=Manifold -D 'target="front"' -o "$TMP/front.stl" "$SCAD" >"$TMP/front.log" 2>&1
front_rc=$?
set -e
if test "$front_rc" -eq 0 || test -s "$TMP/front.stl"; then
  echo "FAIL  PCB assembly intersects front shell" >&2
  exit 1
fi
grep -Fq 'top level object is empty' "$TMP/front.log" || grep -Fq 'Current top level object is empty' "$TMP/front.log" || {
  cat "$TMP/front.log" >&2
  echo "FAIL  front collision check did not prove an empty intersection" >&2
  exit 1
}

openscad --backend=Manifold -D 'target="rear"' -o "$TMP/rear.stl" "$SCAD" >"$TMP/rear.log" 2>&1
test -s "$TMP/rear.stl" || { echo "FAIL  rear support-contact mesh missing" >&2; exit 1; }

read -r zmin zmax <<<"$(awk '
  /vertex/ {
    z=$4
    if (!n++) { zmin=z; zmax=z }
    if (z<zmin) zmin=z
    if (z>zmax) zmax=z
  }
  END { printf "%.6f %.6f", zmin, zmax }
' "$TMP/rear.stl")"

awk -v lo="$zmin" -v hi="$zmax" 'BEGIN { exit !((hi-lo)<=0.011 && lo>=10.189 && hi<=10.211) }' || {
  echo "FAIL  rear-shell collision extends beyond PCB support plane: z=$zmin..$zmax" >&2
  exit 1
}

echo "PASS  real PCB assembly clears both shells; rear contact is support-plane only"
