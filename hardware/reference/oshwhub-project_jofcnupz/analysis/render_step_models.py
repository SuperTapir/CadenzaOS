#!/usr/bin/env python3
"""Measure and render the frozen reference enclosure STEP solids.

Requires the isolated OpenCascade bindings installed in
``/Users/tapir/.cache/cadenza-cad-tools``.  The script never writes to or
modifies the source STEP files.
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path


OCP_CACHE = Path("/Users/tapir/.cache/cadenza-cad-tools")
ROOT = Path(__file__).resolve().parents[1]


def load_ocp():
    if str(OCP_CACHE) not in sys.path:
        sys.path.insert(0, str(OCP_CACHE))
    from OCP.Bnd import Bnd_Box
    from OCP.BRep import BRep_Tool
    from OCP.BRepBndLib import BRepBndLib
    from OCP.BRepGProp import BRepGProp
    from OCP.BRepMesh import BRepMesh_IncrementalMesh
    from OCP.GProp import GProp_GProps
    from OCP.IFSelect import IFSelect_RetDone
    from OCP.STEPControl import STEPControl_Reader
    from OCP.TopAbs import TopAbs_FACE, TopAbs_SHELL, TopAbs_SOLID
    from OCP.TopExp import TopExp_Explorer
    from OCP.TopLoc import TopLoc_Location
    from OCP.TopoDS import TopoDS

    return locals()


def count_subshapes(api, shape, kind) -> int:
    explorer = api["TopExp_Explorer"](shape, kind)
    count = 0
    while explorer.More():
        count += 1
        explorer.Next()
    return count


def mesh_triangles(api, shape, deflection: float = 0.08):
    api["BRepMesh_IncrementalMesh"](shape, deflection, False, 0.35, True)
    vertices: list[tuple[float, float, float]] = []
    triangles: list[tuple[int, int, int]] = []
    explorer = api["TopExp_Explorer"](shape, api["TopAbs_FACE"])
    while explorer.More():
        face = api["TopoDS"].Face_s(explorer.Current())
        location = api["TopLoc_Location"]()
        tri = api["BRep_Tool"].Triangulation_s(face, location)
        if tri is not None:
            transform = location.Transformation()
            base = len(vertices)
            for index in range(1, tri.NbNodes() + 1):
                point = tri.Node(index).Transformed(transform)
                vertices.append((point.X(), point.Y(), point.Z()))
            for index in range(1, tri.NbTriangles() + 1):
                a, b, c = tri.Triangle(index).Get()
                triangles.append((base + a - 1, base + b - 1, base + c - 1))
        explorer.Next()
    return vertices, triangles


def render_png(vertices, triangles, output: Path, elevation: float, azimuth: float) -> None:
    import matplotlib

    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from mpl_toolkits.mplot3d.art3d import Poly3DCollection

    faces = [[vertices[a], vertices[b], vertices[c]] for a, b, c in triangles]
    fig = plt.figure(figsize=(12, 6.5), dpi=160)
    ax = fig.add_subplot(111, projection="3d")
    mesh = Poly3DCollection(
        faces,
        facecolor=(0.72, 0.76, 0.80, 1.0),
        edgecolor=(0.13, 0.16, 0.19, 0.18),
        linewidth=0.08,
    )
    ax.add_collection3d(mesh)
    xs, ys, zs = zip(*vertices)
    mins = (min(xs), min(ys), min(zs))
    maxs = (max(xs), max(ys), max(zs))
    centers = tuple((a + b) / 2 for a, b in zip(mins, maxs))
    radius = max(b - a for a, b in zip(mins, maxs)) / 2
    ax.set_xlim(centers[0] - radius, centers[0] + radius)
    ax.set_ylim(centers[1] - radius, centers[1] + radius)
    ax.set_zlim(centers[2] - radius, centers[2] + radius)
    ax.set_box_aspect((1, 1, 1))
    ax.view_init(elev=elevation, azim=azimuth)
    ax.set_axis_off()
    fig.patch.set_facecolor("#eef1f5")
    ax.set_facecolor("#eef1f5")
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, bbox_inches="tight", pad_inches=0.08)
    plt.close(fig)


def analyze(api, step_path: Path, output_dir: Path) -> dict:
    reader = api["STEPControl_Reader"]()
    status = reader.ReadFile(str(step_path))
    if status != api["IFSelect_RetDone"]:
        raise RuntimeError(f"Failed to read {step_path}: {status}")
    transferred = reader.TransferRoots()
    shape = reader.OneShape()
    if shape.IsNull():
        raise RuntimeError(f"STEP produced a null shape: {step_path}")

    box = api["Bnd_Box"]()
    api["BRepBndLib"].Add_s(shape, box)
    xmin, ymin, zmin, xmax, ymax, zmax = box.Get()
    props = api["GProp_GProps"]()
    api["BRepGProp"].VolumeProperties_s(shape, props)
    center = props.CentreOfMass()
    vertices, triangles = mesh_triangles(api, shape)

    stem = step_path.stem
    render_png(vertices, triangles, output_dir / f"{stem}-top.png", 90, -90)
    render_png(vertices, triangles, output_dir / f"{stem}-iso.png", 32, -58)

    return {
        "source": str(step_path),
        "read_ok": True,
        "transferred_roots": transferred,
        "solids": count_subshapes(api, shape, api["TopAbs_SOLID"]),
        "shells": count_subshapes(api, shape, api["TopAbs_SHELL"]),
        "faces": count_subshapes(api, shape, api["TopAbs_FACE"]),
        "bbox_mm": {
            "xmin": xmin,
            "ymin": ymin,
            "zmin": zmin,
            "xmax": xmax,
            "ymax": ymax,
            "zmax": zmax,
            "width": xmax - xmin,
            "height": ymax - ymin,
            "depth": zmax - zmin,
        },
        "volume_mm3": props.Mass(),
        "centroid_mm": [center.X(), center.Y(), center.Z()],
        "mesh_vertices": len(vertices),
        "mesh_triangles": len(triangles),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=ROOT / "analysis/mechanical-render",
    )
    parser.add_argument(
        "steps",
        type=Path,
        nargs="*",
        default=[ROOT / "顶盖V7.step", ROOT / "底盒V7.step"],
    )
    args = parser.parse_args()
    api = load_ocp()
    results = [analyze(api, p.resolve(), args.output_dir) for p in args.steps]
    args.output_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "step-analysis.json").write_text(
        json.dumps(results, ensure_ascii=False, indent=2) + "\n"
    )
    print(json.dumps(results, ensure_ascii=False, indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
