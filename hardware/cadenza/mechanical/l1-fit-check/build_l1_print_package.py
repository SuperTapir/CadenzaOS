#!/usr/bin/env python3
"""Build and audit the three STL files needed for an L1 enclosure trial print."""

from __future__ import annotations

import hashlib
import json
import math
import shutil
import struct
import sys
import zipfile
from collections import Counter
from pathlib import Path


HERE = Path(__file__).resolve().parent
REPO = HERE.parents[3]
GENERATED = HERE / "generated"
OUTPUT = GENERATED / "print-package"
REFERENCE_BOTTOM = REPO / "hardware/reference/oshwhub-project_jofcnupz/底盒V7.step"
OCP_CACHE = Path("/Users/tapir/.cache/cadenza-cad-tools-py313")


def load_ocp():
    if str(OCP_CACHE) not in sys.path:
        sys.path.insert(0, str(OCP_CACHE))
    from OCP.BRepCheck import BRepCheck_Analyzer
    from OCP.BRepMesh import BRepMesh_IncrementalMesh
    from OCP.IFSelect import IFSelect_RetDone
    from OCP.STEPControl import STEPControl_Reader
    from OCP.StlAPI import StlAPI_Writer
    return locals()


def step_to_stl(api, source: Path, target: Path) -> None:
    reader = api["STEPControl_Reader"]()
    if reader.ReadFile(str(source)) != api["IFSelect_RetDone"]:
        raise RuntimeError(f"cannot read STEP: {source}")
    reader.TransferRoots()
    shape = reader.OneShape()
    if shape.IsNull() or not api["BRepCheck_Analyzer"](shape).IsValid():
        raise RuntimeError(f"invalid STEP: {source}")
    api["BRepMesh_IncrementalMesh"](shape, 0.06, False, 0.3, True)
    if not api["StlAPI_Writer"]().Write(shape, str(target)):
        raise RuntimeError(f"cannot write STL: {target}")


def load_stl(path: Path):
    data = path.read_bytes()
    if len(data) >= 84:
        count = struct.unpack_from("<I", data, 80)[0]
        if len(data) == 84 + 50 * count:
            triangles = []
            offset = 84
            for _ in range(count):
                values = struct.unpack_from("<12fH", data, offset)
                triangles.append((values[3:6], values[6:9], values[9:12]))
                offset += 50
            return triangles

    vertices = []
    for line in data.decode("ascii").splitlines():
        fields = line.strip().split()
        if len(fields) == 4 and fields[0].lower() == "vertex":
            vertices.append(tuple(float(value) for value in fields[1:]))
    if not vertices or len(vertices) % 3:
        raise RuntimeError(f"not a supported STL: {path}")
    return [tuple(vertices[index:index + 3]) for index in range(0, len(vertices), 3)]


def q(vertex):
    return tuple(round(value, 5) for value in vertex)


def audit_stl(path: Path) -> dict:
    triangles = load_stl(path)
    edges = Counter()
    degenerate = 0
    signed_volume = 0.0
    mins = [math.inf, math.inf, math.inf]
    maxs = [-math.inf, -math.inf, -math.inf]
    for a, b, c in triangles:
        qa, qb, qc = q(a), q(b), q(c)
        for vertex in (a, b, c):
            for axis in range(3):
                mins[axis] = min(mins[axis], vertex[axis])
                maxs[axis] = max(maxs[axis], vertex[axis])
        ab = tuple(b[i] - a[i] for i in range(3))
        ac = tuple(c[i] - a[i] for i in range(3))
        cross = (
            ab[1] * ac[2] - ab[2] * ac[1],
            ab[2] * ac[0] - ab[0] * ac[2],
            ab[0] * ac[1] - ab[1] * ac[0],
        )
        if sum(value * value for value in cross) < 1e-16:
            degenerate += 1
        signed_volume += (
            a[0] * (b[1] * c[2] - b[2] * c[1])
            - a[1] * (b[0] * c[2] - b[2] * c[0])
            + a[2] * (b[0] * c[1] - b[1] * c[0])
        ) / 6.0
        for left, right in ((qa, qb), (qb, qc), (qc, qa)):
            edges[tuple(sorted((left, right)))] += 1
    nonmanifold = sum(1 for count in edges.values() if count != 2)
    return {
        "file": path.name,
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "triangles": len(triangles),
        "degenerate_triangles": degenerate,
        "nonmanifold_edges": nonmanifold,
        "watertight_edge_test": nonmanifold == 0,
        "absolute_signed_volume_mm3": abs(signed_volume),
        "bbox_mm": [round(v, 5) for v in (*mins, *maxs)],
    }


def main() -> int:
    api = load_ocp()
    OUTPUT.mkdir(parents=True, exist_ok=True)
    targets = {
        "Cadenza-L1-front-shell.stl": GENERATED / "l1-front-fitcheck.stl",
        "Cadenza-L1-screen-retainer.stl": GENERATED / "l1-screen-retainer-fitcheck.stl",
    }
    for name, source in targets.items():
        shutil.copy2(source, OUTPUT / name)
    step_to_stl(api, REFERENCE_BOTTOM, OUTPUT / "Cadenza-L1-bottom-shell.stl")

    results = [audit_stl(OUTPUT / name) for name in sorted([
        *targets, "Cadenza-L1-bottom-shell.stl"
    ])]
    checks = {
        "three_print_parts_present": len(results) == 3,
        "all_meshes_nonempty": all(item["triangles"] > 0 and item["absolute_signed_volume_mm3"] > 0 for item in results),
        "all_meshes_have_no_degenerate_triangles": all(item["degenerate_triangles"] == 0 for item in results),
        "all_meshes_pass_watertight_edge_test": all(item["watertight_edge_test"] for item in results),
    }
    report = {
        "schema_version": 1,
        "status": "PASS_TRIAL_PRINT" if all(checks.values()) else "FAIL",
        "scope": "mesh integrity and package completeness; physical FPC/fit remain unverified",
        "checks": checks,
        "parts": results,
        "print_units": "millimetres",
        "recommended_first_print": {
            "front_shell": "print first as a sacrificial fit-check before committing to the complete enclosure",
            "bottom_shell": "unchanged reference V7 geometry",
            "retainer": "loose-fit prototype; use removable tape for the first fit test",
        },
    }
    (OUTPUT / "print-package-audit.json").write_text(
        json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    shutil.copy2(HERE / "PRINT_PACKAGE.md", OUTPUT / "README.md")
    package_files = sorted(path for path in OUTPUT.iterdir() if path.is_file())
    (OUTPUT / "SHA256SUMS").write_text(
        "".join(f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n" for path in package_files),
        encoding="utf-8",
    )
    zip_path = GENERATED / "Cadenza-L1-3D-print-package.zip"
    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(OUTPUT.iterdir()):
            if path.is_file():
                archive.write(path, path.name)
    print(json.dumps(report, ensure_ascii=False))
    return 0 if report["status"] != "FAIL" else 1


if __name__ == "__main__":
    raise SystemExit(main())
