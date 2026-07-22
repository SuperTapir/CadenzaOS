#!/usr/bin/env python3
"""Extract a stable comparison baseline from an EasyEDA Pro V2 archive.

The script is intentionally read-only with respect to the unpacked reference
project. It emits JSON and Markdown summaries into a separate analysis folder.
Coordinates in EasyEDA Pro V2 PCB records are expressed in mil; reports also
include millimetres using 1 mil = 0.0254 mm.
"""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
from pathlib import Path
from typing import Any, Iterable


MIL_TO_MM = 0.0254


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_records(path: Path) -> list[list[Any]]:
    records: list[list[Any]] = []
    with path.open("r", encoding="utf-8") as handle:
        for number, line in enumerate(handle, 1):
            line = line.strip()
            if not line:
                continue
            try:
                value = json.loads(line)
            except json.JSONDecodeError as exc:
                raise ValueError(f"{path}:{number}: invalid JSON record") from exc
            if not isinstance(value, list) or not value:
                raise ValueError(f"{path}:{number}: unexpected record")
            records.append(value)
    return records


def by_type(records: Iterable[list[Any]]) -> dict[str, list[list[Any]]]:
    result: dict[str, list[list[Any]]] = collections.defaultdict(list)
    for record in records:
        result[str(record[0])].append(record)
    return dict(result)


def schematic_attributes(records: Iterable[list[Any]]) -> dict[str, dict[str, Any]]:
    attrs: dict[str, dict[str, Any]] = collections.defaultdict(dict)
    for record in records:
        if record[0] == "ATTR":
            attrs[str(record[2])][str(record[3])] = record[4]
    return dict(attrs)


def pcb_attributes(records: Iterable[list[Any]]) -> dict[str, dict[str, Any]]:
    attrs: dict[str, dict[str, Any]] = collections.defaultdict(dict)
    for record in records:
        if record[0] == "ATTR":
            attrs[str(record[3])][str(record[7])] = record[8]
    return dict(attrs)


def mm(value: float) -> float:
    return round(float(value) * MIL_TO_MM, 4)


def extract_board_outline(pcb: dict[str, list[list[Any]]]) -> dict[str, Any]:
    candidates: list[dict[str, Any]] = []
    for record in pcb.get("POLY", []):
        layer = record[4]
        geometry = record[6]
        if layer != 11 or not isinstance(geometry, list) or not geometry:
            continue
        if geometry[0] == "R" and len(geometry) >= 7:
            # EasyEDA Pro V2 rectangle geometry is x, y, width, height,
            # rotation, radius. PCB Y increases from the lower edge toward
            # the rectangle's Y origin, so y - height is the lower edge.
            x, y, width, height = map(float, geometry[1:5])
            candidates.append(
                {
                    "record_id": record[1],
                    "kind": "rounded_rectangle",
                    "raw_mil": {"x": x, "y": y, "width": width, "height": height},
                    "width_mm": mm(width),
                    "height_mm": mm(height),
                    "corner_radius_mm": mm(float(geometry[6])),
                }
            )
        else:
            candidates.append({"record_id": record[1], "kind": "other", "raw": geometry})
    return {
        "outline_layer_id": 11,
        "candidates": candidates,
        "primary": max(
            candidates,
            key=lambda item: item.get("width_mm", 0) * item.get("height_mm", 0),
            default=None,
        ),
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("unpacked", type=Path, help="EasyEDA Pro V2 unpacked directory")
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()

    root = args.unpacked.resolve()
    project_path = root / "project.json"
    schematic_paths = sorted(root.rglob("*.esch"))
    pcb_paths = sorted(root.rglob("*.epcb"))
    if not project_path.is_file() or len(schematic_paths) != 1 or len(pcb_paths) != 1:
        raise SystemExit("expected project.json, exactly one .esch, and exactly one .epcb")

    schematic_path = schematic_paths[0]
    pcb_path = pcb_paths[0]
    project = json.loads(project_path.read_text(encoding="utf-8"))
    sch_records = load_records(schematic_path)
    pcb_records = load_records(pcb_path)
    sch = by_type(sch_records)
    pcb = by_type(pcb_records)
    sch_attrs = schematic_attributes(sch_records)
    pcb_attrs = pcb_attributes(pcb_records)

    schematic_components: list[dict[str, Any]] = []
    for record in sch.get("COMPONENT", []):
        owner = str(record[1])
        attrs = sch_attrs.get(owner, {})
        designator = attrs.get("Designator")
        if not designator:
            continue
        device_id = attrs.get("Device")
        device = project.get("devices", {}).get(device_id, {})
        device_attrs = device.get("attributes", {})
        schematic_components.append(
            {
                "id": owner,
                "designator": designator,
                "instance_name": record[2],
                "device_id": device_id,
                "value": attrs.get("Value") or device_attrs.get("Value") or attrs.get("Name"),
                "manufacturer": device_attrs.get("Manufacturer"),
                "mpn": device_attrs.get("Manufacturer Part"),
                "supplier_part": device_attrs.get("Supplier Part"),
                "part_class": device_attrs.get("JLCPCB Part Class"),
                "footprint": attrs.get("Footprint") or device_attrs.get("Footprint"),
                "x_raw": record[3],
                "y_raw": record[4],
                "rotation_deg": record[5],
            }
        )

    pcb_components: list[dict[str, Any]] = []
    for record in pcb.get("COMPONENT", []):
        owner = str(record[1])
        attrs = pcb_attrs.get(owner, {})
        pcb_components.append(
            {
                "id": owner,
                "designator": attrs.get("Designator"),
                "device_id": attrs.get("Device"),
                "footprint": attrs.get("Footprint"),
                "side_code": record[2],
                "layer_id": record[3],
                "x_mil": record[4],
                "y_mil": record[5],
                "x_mm_absolute": mm(record[4]),
                "y_mm_absolute": mm(record[5]),
                "rotation_deg": record[6],
            }
        )

    outline = extract_board_outline(pcb)
    primary = outline.get("primary") or {}
    raw_box = primary.get("raw_mil") or {}
    x0, y_top = raw_box.get("x"), raw_box.get("y")
    height = raw_box.get("height")
    if x0 is not None and y_top is not None and height is not None:
        y_bottom = float(y_top) - float(height)
        for component in pcb_components:
            component["x_mm_from_left"] = mm(float(component["x_mil"]) - float(x0))
            component["y_mm_from_bottom"] = mm(float(component["y_mil"]) - y_bottom)
            component["y_mm_from_top"] = mm(float(y_top) - float(component["y_mil"]))

    nets = [str(record[1]) for record in pcb.get("NET", [])]
    track_lines = [
        record
        for record in pcb.get("LINE", [])
        if record[3] and record[4] in (1, 2)
    ]
    tracks_by_layer = collections.Counter(str(record[4]) for record in track_lines)
    physical_copper_layers = [
        record for record in pcb.get("LAYER_PHYS", []) if record[1] in (1, 2)
    ]
    mounting_holes = [
        component
        for component in pcb_components
        if str(component.get("designator") or "").upper().startswith("SCREW")
    ]
    key_refs = {
        "U1", "U2", "U3", "U4", "U5", "U6", "U7", "USB1", "FPC1",
        "CARD1", "CN1", "H1", "SW1", "SW2", "BOOT1", "RST1",
        "SCREW1", "SCREW2", "SCREW3", "SCREW4",
    }
    critical_positions = [
        component for component in pcb_components if component.get("designator") in key_refs
    ]

    baseline = {
        "schema_version": 1,
        "source": {
            "root": str(root),
            "project": str(project_path.relative_to(root)),
            "schematic": str(schematic_path.relative_to(root)),
            "pcb": str(pcb_path.relative_to(root)),
            "sha256": {
                "project": sha256(project_path),
                "schematic": sha256(schematic_path),
                "pcb": sha256(pcb_path),
            },
        },
        "project_title": project.get("config", {}).get("title"),
        "record_counts": {
            "schematic": dict(sorted(collections.Counter(r[0] for r in sch_records).items())),
            "pcb": dict(sorted(collections.Counter(r[0] for r in pcb_records).items())),
        },
        "schematic": {
            "all_component_records": len(sch.get("COMPONENT", [])),
            "designated_components": len(schematic_components),
            "components": sorted(schematic_components, key=lambda item: item["designator"]),
        },
        "pcb": {
            "components": len(pcb_components),
            "nets": len(nets),
            "net_names": nets,
            "copper_layers": len(physical_copper_layers),
            "copper_layer_ids": [record[1] for record in physical_copper_layers],
            "vias": len(pcb.get("VIA", [])),
            "routed_track_segments": len(track_lines),
            "track_segments_by_layer": dict(sorted(tracks_by_layer.items())),
            "pours": len(pcb.get("POUR", [])),
            "poured_fragments": len(pcb.get("POURED", [])),
            "board_outline": outline,
            "mounting_holes": mounting_holes,
            "critical_positions": sorted(
                critical_positions, key=lambda item: str(item.get("designator"))
            ),
            "component_list": sorted(
                pcb_components, key=lambda item: str(item.get("designator"))
            ),
        },
    }

    args.output_dir.mkdir(parents=True, exist_ok=True)
    json_path = args.output_dir / "easyeda-v2-baseline.json"
    md_path = args.output_dir / "easyeda-v2-baseline.md"
    json_path.write_text(json.dumps(baseline, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    lines = [
        "# EasyEDA Pro V2 转换前基线",
        "",
        f"- 项目：`{baseline['project_title']}`",
        f"- 原理图全部 COMPONENT 记录：{baseline['schematic']['all_component_records']}",
        f"- 原理图带位号组件：{baseline['schematic']['designated_components']}",
        f"- PCB 组件：{baseline['pcb']['components']}",
        f"- PCB 网络：{baseline['pcb']['nets']}",
        f"- 物理铜层：{baseline['pcb']['copper_layers']}",
        f"- 过孔：{baseline['pcb']['vias']}",
        f"- 有网络铜走线段：{baseline['pcb']['routed_track_segments']}",
        f"- 铺铜对象/碎片：{baseline['pcb']['pours']} / {baseline['pcb']['poured_fragments']}",
    ]
    if primary:
        lines.extend(
            [
                f"- 主板框：{primary['width_mm']} × {primary['height_mm']} mm",
                f"- 板框圆角：R{primary['corner_radius_mm']} mm",
            ]
        )
    lines.extend(["", "## 安装孔", "", "| 位号 | X/mm | Y/mm |", "|---|---:|---:|"])
    for item in mounting_holes:
        lines.append(
            f"| {item['designator']} | {item.get('x_mm_from_left', '')} | "
            f"{item.get('y_mm_from_bottom', '')} |"
        )
    lines.extend(["", "## 关键器件坐标", "", "| 位号 | X/mm | Y/mm | 旋转 | 封装 ID |", "|---|---:|---:|---:|---|"])
    for item in sorted(critical_positions, key=lambda value: str(value.get("designator"))):
        lines.append(
            f"| {item['designator']} | {item.get('x_mm_from_left', '')} | "
            f"{item.get('y_mm_from_bottom', '')} | {item['rotation_deg']} | "
            f"`{item.get('footprint') or ''}` |"
        )
    lines.extend(
        [
            "",
            "## 用途",
            "",
            "本文件只描述 EasyEDA Pro V2 原工程。KiCad 导入后的统计必须与对应字段比较；",
            "任何差异需要解释，不能把本文件本身当作电气正确性证明。",
        ]
    )
    md_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print(json_path)
    print(md_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
