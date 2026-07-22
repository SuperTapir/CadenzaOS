#!/usr/bin/env python3
"""Compare the untouched KiCad EasyEDA-Pro PCB import with the V2 baseline.

Run with KiCad's bundled Python so the pcbnew module is available:

    PYTHONDONTWRITEBYTECODE=1 \
      /Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
      analysis/analyze_kicad_import.py
"""

from __future__ import annotations

import json
import math
import sys
import argparse
from collections import Counter
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BASELINE_PATH = ROOT / "analysis/baseline/easyeda-v2-baseline.json"
IMPORT_DIR = ROOT / "kicad-import-smoke-test"
PCB_PATH = IMPORT_DIR / "reference-imported.kicad_pcb"
JSON_PATH = IMPORT_DIR / "pcb-difference-audit.json"
MD_PATH = IMPORT_DIR / "PCB_DIFFERENCE_AUDIT.md"
KICAD_SITE_PACKAGES = Path(
    "/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/"
    "lib/python3.9/site-packages"
)


def mm(pcbnew, value: int | float) -> float:
    return round(float(pcbnew.ToMM(value)), 6)


def angle_delta(a: float, b: float) -> float:
    raw = abs((a - b) % 360.0)
    return min(raw, 360.0 - raw)


def board_centerline_extent(pcbnew, board):
    """Return Edge.Cuts centerline bounds, excluding graphic line width."""
    edges = [d for d in board.GetDrawings() if d.GetLayer() == pcbnew.Edge_Cuts]
    if not edges:
        raise RuntimeError("No Edge.Cuts graphics found")

    left = math.inf
    top = math.inf
    right = -math.inf
    bottom = -math.inf
    for edge in edges:
        box = edge.GetBoundingBox()
        half_width = edge.GetWidth() / 2 if hasattr(edge, "GetWidth") else 0
        left = min(left, box.GetX() + half_width)
        top = min(top, box.GetY() + half_width)
        right = max(right, box.GetRight() - half_width)
        bottom = max(bottom, box.GetBottom() - half_width)

    return {
        "left_mm": mm(pcbnew, left),
        "top_mm": mm(pcbnew, top),
        "right_mm": mm(pcbnew, right),
        "bottom_mm": mm(pcbnew, bottom),
        "width_mm": mm(pcbnew, right - left),
        "height_mm": mm(pcbnew, bottom - top),
        "edge_graphics": len(edges),
    }


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Compare a KiCad EasyEDA-Pro PCB import with the frozen V2 baseline."
    )
    parser.add_argument("--pcb", type=Path, default=PCB_PATH, help="KiCad PCB to audit")
    parser.add_argument("--json", type=Path, default=JSON_PATH, help="JSON report path")
    parser.add_argument("--markdown", type=Path, default=MD_PATH, help="Markdown report path")
    args = parser.parse_args(argv)

    if str(KICAD_SITE_PACKAGES) not in sys.path:
        sys.path.insert(0, str(KICAD_SITE_PACKAGES))
    import pcbnew  # type: ignore

    baseline = json.loads(BASELINE_PATH.read_text())
    source = baseline["pcb"]
    pcb_path = args.pcb.resolve()
    board = pcbnew.LoadBoard(str(pcb_path))
    extent = board_centerline_extent(pcbnew, board)

    footprints = list(board.GetFootprints())
    tracks = list(board.GetTracks())
    vias = [item for item in tracks if isinstance(item, pcbnew.PCB_VIA)]
    routed_segments = [item for item in tracks if not isinstance(item, pcbnew.PCB_VIA)]
    zones = list(board.Zones())
    net_names = sorted(
        net.GetNetname()
        for code, net in board.GetNetInfo().NetsByNetcode().items()
        if int(code) != 0 and net.GetNetname()
    )

    imported = {
        "components": len(footprints),
        "nets": len(net_names),
        "net_names": net_names,
        "copper_layers": board.GetCopperLayerCount(),
        "vias": len(vias),
        "routed_track_segments": len(routed_segments),
        "zones": len(zones),
        "board_outline": extent,
    }

    checks = []

    def exact_check(name: str, source_value, imported_value):
        checks.append(
            {
                "name": name,
                "source": source_value,
                "imported": imported_value,
                "status": "PASS" if source_value == imported_value else "FAIL",
            }
        )

    exact_check("components", source["components"], imported["components"])
    exact_check("nets", source["nets"], imported["nets"])
    exact_check("net_name_set", sorted(source["net_names"]), imported["net_names"])
    exact_check("copper_layers", source["copper_layers"], imported["copper_layers"])
    exact_check("vias", source["vias"], imported["vias"])
    exact_check(
        "routed_track_segments",
        source["routed_track_segments"],
        imported["routed_track_segments"],
    )

    source_outline = source["board_outline"]["primary"]
    for axis in ("width_mm", "height_mm"):
        delta = round(imported["board_outline"][axis] - source_outline[axis], 6)
        checks.append(
            {
                "name": f"board_{axis}",
                "source": source_outline[axis],
                "imported": imported["board_outline"][axis],
                "delta_mm": delta,
                "status": "PASS" if abs(delta) <= 0.001 else "FAIL",
            }
        )

    position_audit = []
    for expected in source["critical_positions"]:
        ref = expected["designator"]
        footprint = board.FindFootprintByReference(ref)
        if footprint is None:
            position_audit.append({"designator": ref, "status": "FAIL", "reason": "missing"})
            continue

        pos = footprint.GetPosition()
        x_from_left = mm(pcbnew, pos.x) - extent["left_mm"]
        y_from_bottom = extent["bottom_mm"] - mm(pcbnew, pos.y)
        orientation = float(footprint.GetOrientationDegrees()) % 360.0
        dx = round(x_from_left - expected["x_mm_from_left"], 6)
        dy = round(y_from_bottom - expected["y_mm_from_bottom"], 6)
        da = round(angle_delta(orientation, float(expected["rotation_deg"])), 6)
        status = "PASS" if max(abs(dx), abs(dy)) <= 0.001 and da <= 0.001 else "FAIL"
        position_audit.append(
            {
                "designator": ref,
                "source_x_from_left_mm": expected["x_mm_from_left"],
                "imported_x_from_left_mm": round(x_from_left, 6),
                "dx_mm": dx,
                "source_y_from_bottom_mm": expected["y_mm_from_bottom"],
                "imported_y_from_bottom_mm": round(y_from_bottom, 6),
                "dy_mm": dy,
                "source_rotation_deg": expected["rotation_deg"],
                "imported_rotation_deg": orientation,
                "rotation_delta_deg": da,
                "status": status,
            }
        )

    zone_layers = Counter(board.GetLayerName(zone.GetLayer()) for zone in zones)
    zone_nets = Counter(zone.GetNetname() or "<no net>" for zone in zones)
    zone_audit = {
        "source_pour_definitions": source["pours"],
        "source_poured_fragments": source["poured_fragments"],
        "imported_filled_zone_polygons": len(zones),
        "imported_by_layer": dict(sorted(zone_layers.items())),
        "imported_distinct_nets": len(zone_nets),
        "status": "MANUAL_REVIEW",
        "reason": (
            "KiCad imports EasyEDA filled copper fragments as individual filled zones; "
            "the object counts are not semantically equivalent. Render and connectivity review required."
        ),
    }

    result = {
        "source": str(BASELINE_PATH),
        "imported_board": str(pcb_path),
        "kicad_version": pcbnew.GetBuildVersion(),
        "imported": imported,
        "checks": checks,
        "critical_position_audit": position_audit,
        "zone_audit": zone_audit,
        "summary": {
            "automatic_pass": sum(c["status"] == "PASS" for c in checks)
            + sum(c["status"] == "PASS" for c in position_audit),
            "automatic_fail": sum(c["status"] == "FAIL" for c in checks)
            + sum(c["status"] == "FAIL" for c in position_audit),
            "manual_review": 1,
        },
    }
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")

    lines = [
        "# EasyEDA Pro V2 → KiCad PCB 差异审计",
        "",
        f"- KiCad：`{result['kicad_version']}`",
        f"- 原始基线：`{BASELINE_PATH.relative_to(ROOT)}`",
        f"- 未修改导入板：`{pcb_path.relative_to(ROOT) if pcb_path.is_relative_to(ROOT) else pcb_path}`",
        f"- 自动通过：{result['summary']['automatic_pass']}",
        f"- 自动失败：{result['summary']['automatic_fail']}",
        "- 人工复核：铺铜几何与 3D/丝印/文本表现",
        "",
        "## 数量与板框",
        "",
        "| 检查 | 原工程 | KiCad 导入 | 结果 |",
        "|---|---:|---:|---|",
    ]
    for check in checks:
        source_text = check["source"]
        imported_text = check["imported"]
        if check["name"] == "net_name_set":
            source_text = f"{len(check['source'])} names"
            imported_text = f"{len(check['imported'])} names"
        lines.append(
            f"| {check['name']} | {source_text} | {imported_text} | {check['status']} |"
        )

    lines += [
        "",
        "## 关键器件与安装孔坐标",
        "",
        "坐标均相对板框左下角；位置容差为 0.001 mm，角度容差为 0.001°。",
        "",
        "| 位号 | ΔX/mm | ΔY/mm | Δ角度/° | 结果 |",
        "|---|---:|---:|---:|---|",
    ]
    for item in position_audit:
        if item["status"] == "FAIL" and "reason" in item:
            lines.append(f"| {item['designator']} | — | — | — | FAIL ({item['reason']}) |")
        else:
            lines.append(
                f"| {item['designator']} | {item['dx_mm']:.6f} | {item['dy_mm']:.6f} | "
                f"{item['rotation_delta_deg']:.6f} | {item['status']} |"
            )

    lines += [
        "",
        "## 铺铜转换",
        "",
        f"- 原工程：{source['pours']} 个铺铜定义、{source['poured_fragments']} 个已记录填充碎片。",
        f"- KiCad：{len(zones)} 个导入的已填充 zone polygon。",
        f"- KiCad 分层：{dict(sorted(zone_layers.items()))}。",
        "- 结论：`MANUAL_REVIEW`。两个格式的对象计数语义不同，不能据数量直接判定丢失；必须渲染正反面并复核 GND/电源连续性。",
        "",
        "## 当前结论",
        "",
    ]
    if result["summary"]["automatic_fail"] == 0:
        lines.append(
            "组件、网络、网络名、铜层、过孔、走线段、板框和关键坐标全部自动匹配。"
            "PCB 导入通过结构化数据门禁，但在铺铜渲染和完整原理图导入完成前，尚不能成为派生 golden 工程。"
        )
    else:
        lines.append(
            "存在自动差异，必须在继续派生前解释或修复；当前 PCB 导入未通过转换门禁。"
        )
    args.markdown.parent.mkdir(parents=True, exist_ok=True)
    args.markdown.write_text("\n".join(lines) + "\n")

    print(json.dumps(result["summary"], ensure_ascii=False))
    return 0 if result["summary"]["automatic_fail"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
