#!/usr/bin/env python3
"""Extract an auditable mechanical inventory from the frozen enclosure STEP files.

The STEP files are read-only inputs.  The output deliberately separates exact BREP
measurements from functional interpretations (for example, "battery pocket"),
because the STEP files do not contain feature names or parametric CAD history.
"""

from __future__ import annotations

import hashlib
import json
import sys
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "analysis/mechanical-internal-baseline.json"
OCP_CACHES = (
    Path("/Users/tapir/.cache/cadenza-cad-tools-py313"),
    Path("/Users/tapir/.cache/cadenza-cad-tools"),
)


def load_ocp():
    for cache in OCP_CACHES:
        if cache.exists():
            sys.path.insert(0, str(cache))
            break
    from OCP.Bnd import Bnd_Box
    from OCP.BRepAdaptor import BRepAdaptor_Surface
    from OCP.BRepBndLib import BRepBndLib
    from OCP.BRepGProp import BRepGProp
    from OCP.GProp import GProp_GProps
    from OCP.IFSelect import IFSelect_RetDone
    from OCP.STEPControl import STEPControl_Reader
    from OCP.TopAbs import TopAbs_FACE, TopAbs_SOLID
    from OCP.TopExp import TopExp_Explorer
    from OCP.TopoDS import TopoDS

    return locals()


def rounded(value: float) -> float:
    return round(value, 6)


def bbox(api, shape) -> dict[str, float]:
    box = api["Bnd_Box"]()
    api["BRepBndLib"].Add_s(shape, box)
    xmin, ymin, zmin, xmax, ymax, zmax = box.Get()
    return {
        "xmin": rounded(xmin),
        "ymin": rounded(ymin),
        "zmin": rounded(zmin),
        "xmax": rounded(xmax),
        "ymax": rounded(ymax),
        "zmax": rounded(zmax),
        "width": rounded(xmax - xmin),
        "height": rounded(ymax - ymin),
        "depth": rounded(zmax - zmin),
    }


def vector(direction) -> list[float]:
    return [rounded(direction.X()), rounded(direction.Y()), rounded(direction.Z())]


def point(location) -> list[float]:
    return [rounded(location.X()), rounded(location.Y()), rounded(location.Z())]


def analyze_step(api, path: Path) -> dict:
    reader = api["STEPControl_Reader"]()
    status = reader.ReadFile(str(path))
    if status != api["IFSelect_RetDone"]:
        raise RuntimeError(f"Cannot read STEP: {path}")
    reader.TransferRoots()
    shape = reader.OneShape()

    props = api["GProp_GProps"]()
    api["BRepGProp"].VolumeProperties_s(shape, props)
    faces = []
    explorer = api["TopExp_Explorer"](shape, api["TopAbs_FACE"])
    while explorer.More():
        face = api["TopoDS"].Face_s(explorer.Current())
        adaptor = api["BRepAdaptor_Surface"](face)
        kind = str(adaptor.GetType()).split(".")[-1].removeprefix("GeomAbs_").lower()
        face_props = api["GProp_GProps"]()
        api["BRepGProp"].SurfaceProperties_s(face, face_props)
        item = {
            "id": len(faces) + 1,
            "surface": kind,
            "area_mm2": rounded(face_props.Mass()),
            "centroid_mm": point(face_props.CentreOfMass()),
            "bbox_mm": bbox(api, face),
        }
        if kind == "plane":
            plane = adaptor.Plane()
            item["normal"] = vector(plane.Axis().Direction())
        elif kind == "cylinder":
            cylinder = adaptor.Cylinder()
            item["radius_mm"] = rounded(cylinder.Radius())
            item["axis_origin_mm"] = point(cylinder.Axis().Location())
            item["axis_direction"] = vector(cylinder.Axis().Direction())
        elif kind == "cone":
            cone = adaptor.Cone()
            item["reference_radius_mm"] = rounded(cone.RefRadius())
            item["semi_angle_deg"] = rounded(cone.SemiAngle() * 180.0 / 3.141592653589793)
            item["axis_origin_mm"] = point(cone.Axis().Location())
            item["axis_direction"] = vector(cone.Axis().Direction())
        elif kind == "torus":
            torus = adaptor.Torus()
            item["major_radius_mm"] = rounded(torus.MajorRadius())
            item["minor_radius_mm"] = rounded(torus.MinorRadius())
            item["axis_origin_mm"] = point(torus.Axis().Location())
            item["axis_direction"] = vector(torus.Axis().Direction())
        faces.append(item)
        explorer.Next()

    solid_count = 0
    explorer = api["TopExp_Explorer"](shape, api["TopAbs_SOLID"])
    while explorer.More():
        solid_count += 1
        explorer.Next()

    horizontal_levels = Counter()
    for face in faces:
        if face["surface"] != "plane":
            continue
        nx, ny, nz = face["normal"]
        if abs(nz) > 0.999 and abs(nx) < 0.001 and abs(ny) < 0.001:
            horizontal_levels[f'{face["centroid_mm"][2]:.6f}'] += face["area_mm2"]

    return {
        "source": str(path.relative_to(ROOT)),
        "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
        "read_ok": not shape.IsNull(),
        "solids": solid_count,
        "faces": len(faces),
        "surface_type_counts": dict(Counter(face["surface"] for face in faces)),
        "bbox_mm": bbox(api, shape),
        "volume_mm3": rounded(props.Mass()),
        "centroid_mm": point(props.CentreOfMass()),
        "horizontal_planar_levels_area_mm2": {
            key: rounded(value) for key, value in sorted(horizontal_levels.items(), key=lambda x: float(x[0]))
        },
        "face_inventory": faces,
    }


def box(x0, y0, z0, x1, y1, z1) -> dict:
    return {
        "xmin": x0,
        "ymin": y0,
        "zmin": z0,
        "xmax": x1,
        "ymax": y1,
        "zmax": z1,
        "width": round(x1 - x0, 6),
        "height": round(y1 - y0, 6),
        "depth": round(z1 - z0, 6),
    }


def curated_features() -> list[dict]:
    screw_centers = [[5.0, 5.0], [5.0, 55.0], [125.0, 5.0], [125.0, 55.0]]
    return [
        {
            "id": "pcb_coordinate_frame",
            "classification": "derived_from_pcb_and_step_alignment",
            "confidence": "high",
            "geometry": {"pcb_outline_mm": [130.0, 60.0], "interface_plane_z_mm": 0.0},
            "evidence": "Four STEP screw axes exactly match the PCB centers (5,5), (5,55), (125,5), (125,55).",
        },
        {
            "id": "main_pcb_mounting_bosses",
            "classification": "exact_geometry_function_confirmed_by_pcb_alignment",
            "confidence": "high",
            "geometry": {
                "count": 4,
                "centers_xy_mm": screw_centers,
                "outer_radius_mm": 3.5,
                "outer_diameter_mm": 7.0,
                "outer_cylindrical_span_z_mm": [1.6, 12.6],
                "bore_radius_mm": 1.5,
                "bore_diameter_mm": 3.0,
                "bore_cylindrical_span_z_mm": [1.6, 6.6],
                "boss_end_face_z_mm": 6.6,
            },
            "evidence": "Bottom STEP contains four coaxial r=3.5 and r=1.5 cylindrical face pairs at the PCB hole centers.",
            "pending_verification": "STEP geometry shows nominal diameter only; intended screw standard, thread-forming fit and print shrink compensation are not encoded.",
        },
        {
            "id": "pcb_support_plane",
            "classification": "geometric_inference",
            "confidence": "medium",
            "geometry": {"z_mm": 1.6, "nominal_pcb_thickness_mm": 1.6, "cover_interface_z_mm": 0.0},
            "evidence": "The four mounting bosses terminate at z=1.6 while the cover mating plane is z=0; this equals the reference 1.6 mm PCB thickness.",
            "pending_verification": "Confirm with a real assembled unit or CAD assembly; STEP has no named PCB datum.",
        },
        {
            "id": "bottom_inner_floor_and_depth",
            "classification": "exact_geometry",
            "confidence": "high",
            "geometry": {
                "inner_floor_z_mm": 12.6,
                "outer_back_z_mm": 14.6,
                "back_wall_thickness_mm": 2.0,
                "nominal_empty_depth_from_pcb_support_to_floor_mm": 11.0,
                "inner_wall_nominal_bounds_xy_mm": [1.0, 1.0, 129.0, 59.0],
            },
            "evidence": "Dominant planar faces occur at z=12.6 and z=14.6; the support plane is z=1.6.",
        },
        {
            "id": "candidate_battery_retention_volume",
            "classification": "functional_interpretation_of_exact_ribs",
            "confidence": "medium",
            "geometry": {
                "candidate_box_mm": box(38.0, 9.0, 6.6, 114.0, 55.0, 12.6),
                "candidate_size_mm": [76.0, 46.0, 6.0],
                "surrounding_rib_groups_xy_mm": [
                    [35.0, 6.0, 38.0, 9.0],
                    [35.0, 55.0, 38.0, 58.0],
                    [114.0, 6.0, 117.0, 9.0],
                    [114.0, 55.0, 117.0, 58.0],
                ],
                "rib_span_z_mm": [6.6, 12.6],
            },
            "evidence": "Four 3x3 mm rib groups bound a 76x46 mm central-right rectangle and rise 6 mm from the inner floor.",
            "pending_verification": "The STEP has no feature name. Treat 76x46x6 mm only as a candidate battery envelope, not a selected cell size; wire exit, adhesive, protection PCB and swelling allowance remain unknown.",
        },
        {
            "id": "speaker_grille",
            "classification": "exact_geometry_function_supported_by_assembly_image",
            "confidence": "high",
            "geometry": {
                "overall_bbox_mm": box(5.0, 16.48, 12.6, 31.0, 42.5, 14.6),
                "center_xy_mm": [18.5, 29.49],
                "through_back_wall": True,
                "slot_count": 7,
                "rounded_slot_bboxes_xy_mm": [
                    [12.77, 16.48, 24.0, 18.48],
                    [9.5, 20.5, 27.5, 22.5],
                    [7.0, 24.5, 30.0, 26.5],
                    [5.0, 28.5, 31.0, 30.5],
                    [7.0, 32.48, 30.0, 34.48],
                    [9.5, 36.48, 27.5, 38.48],
                    [12.77, 40.5, 24.0, 42.5],
                ],
            },
            "evidence": "Seven 2 mm high rounded apertures traverse z=12.6..14.6; the reference assembly image shows the acoustic grille in this end region.",
        },
        {
            "id": "speaker_retention_posts",
            "classification": "functional_interpretation_of_exact_geometry",
            "confidence": "medium",
            "geometry": {
                "post_centers_xy_mm": [[6.38, 41.5], [30.34, 17.56]],
                "post_outer_bboxes_xy_mm": [[4.38, 39.5, 8.38, 43.5], [28.34, 15.56, 32.34, 19.56]],
                "post_span_z_mm": [6.6, 12.6],
                "central_cylindrical_radius_mm": 1.0,
            },
            "evidence": "Two diagonally opposed 4x4 mm post groups flank the grille and contain r=1 mm cylindrical faces.",
            "pending_verification": "No speaker model is embedded. The geometry suggests a roughly 25 mm class speaker region and does not establish fit for the planned 40 mm speaker.",
        },
        {
            "id": "usb1_aligned_back_aperture",
            "classification": "functional_interpretation_confirmed_by_xy_alignment",
            "confidence": "medium",
            "geometry": {
                "overall_bbox_mm": box(59.68, 1.17, 11.42, 70.32, 6.03, 14.6),
                "center_xy_mm": [65.0, 3.6],
                "nominal_obround_radius_mm": 2.05,
                "flat_span_x_mm": [61.73, 68.27],
                "aligned_pcb_usb1_center_xy_mm": [65.0, 3.6039],
            },
            "evidence": "A rounded opening is centered at x=65,y≈3.6 and aligns with PCB USB1 after the PCB Y axis is mapped into enclosure coordinates.",
            "pending_verification": "Confirm the actual connector insertion direction and accessible clearance with the assembled reference or a fit print.",
        },
        {
            "id": "sw1_edge_aperture",
            "classification": "exact_geometry_function_confirmed_by_xy_alignment",
            "confidence": "high",
            "geometry": {
                "bbox_mm": box(91.34, 59.0, 2.8, 96.45, 61.0, 5.8),
                "aperture_size_xz_mm": [5.11, 3.0],
                "center_xz_mm": [93.895, 4.3],
                "aligned_pcb_sw1_center_xy_mm": [93.8968, 55.8001],
            },
            "evidence": "The bottom STEP rectangular edge opening aligns in X with SW1 and crosses the y=59..61 wall.",
        },
        {
            "id": "microsd_edge_aperture",
            "classification": "functional_interpretation_confirmed_by_component_alignment",
            "confidence": "high",
            "geometry": {
                "full_beveled_bbox_mm": box(32.67, -1.0, 0.65, 46.37, 1.0, 4.2),
                "main_flat_span_x_mm": [33.87, 45.17],
                "aligned_pcb_card1_center_xy_mm": [40.4489, 10.6977],
            },
            "evidence": "The beveled long-edge opening is centered near CARD1 in X and is distinct from the USB1-aligned aperture.",
        },
        {
            "id": "top_screen_window",
            "classification": "exact_geometry",
            "confidence": "high",
            "geometry": {"bbox_xy_mm": [27.63, 3.07, 102.37, 56.93], "size_mm": [74.74, 53.86], "through_span_z_mm": [-3.0, 0.0]},
            "evidence": "Four planar aperture walls span the full 3 mm cover thickness.",
        },
        {
            "id": "top_control_apertures",
            "classification": "exact_geometry_function_confirmed_by_pcb_alignment",
            "confidence": "high",
            "geometry": {
                "round_holes_diameter_mm": 7.41,
                "round_hole_centers_xy_mm": [[10.375, 36.05], [20.875, 29.5], [13.2, 56.0], [22.4, 56.0], [107.6, 56.0], [116.8, 56.0]],
                "sw2_rectangular_hole_xy_mm": [109.0, 24.0, 122.5, 36.0],
                "sw2_rectangular_hole_size_mm": [13.5, 12.0],
                "led2_hole_xy_mm": [114.8, 11.94, 117.0, 14.3],
                "led2_hole_size_mm": [2.2, 2.36],
            },
            "evidence": "Cylinder/plane centers coincide with KEY1..KEY6, SW2 and LED2 after PCB-to-case Y conversion.",
        },
        {
            "id": "top_corner_fastener_holes",
            "classification": "exact_geometry",
            "confidence": "high",
            "geometry": {"count": 4, "centers_xy_mm": screw_centers, "diameter_mm": 3.0, "through_span_z_mm": [-3.0, 0.0]},
            "evidence": "Four r=1.5 cylindrical faces traverse the top cover.",
        },
        {
            "id": "dedicated_microphone_aperture",
            "classification": "not_identified",
            "confidence": "medium",
            "geometry": None,
            "evidence": "No aperture in either STEP was found directly aligned to PCB microphone U7 at enclosure x=15.968,y=11.2722.",
            "pending_verification": "Inspect the real unit and acoustic path; the design may intentionally couple the microphone through another cavity or shared opening.",
        },
    ]


def main() -> int:
    api = load_ocp()
    steps = [ROOT / "顶盖V7.step", ROOT / "底盒V7.step"]
    result = {
        "schema_version": 1,
        "units": "mm",
        "coordinate_convention": {
            "x": "PCB left to right, 0..130",
            "y": "enclosure lower edge to upper edge, 0..60; equals 60 - PCB baseline y_from_top",
            "z": "top/bottom cover mating plane is 0; top cover is negative Z and bottom enclosure is positive Z",
        },
        "method": {
            "kernel": "OpenCascade via cadquery-ocp 7.9.3.1.1",
            "exact_geometry_note": "BREP face inventory is measured from frozen STEP inputs.",
            "interpretation_note": "Functional names are explicitly confidence-labelled because STEP has no feature history.",
        },
        "models": {path.stem: analyze_step(api, path) for path in steps},
        "curated_features": curated_features(),
    }
    OUTPUT.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print(OUTPUT)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
