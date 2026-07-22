#!/usr/bin/env python3
"""Validate BRep, ASCII STL meshes, fit clearances and non-frozen state."""

from __future__ import annotations

import json
import math
import sys
from collections import Counter
from pathlib import Path


OCP_CACHE = Path("/Users/tapir/.cache/cadenza-cad-tools")
HERE = Path(__file__).resolve().parent
MANIFEST = HERE / "generated/printable-controls-manifest.json"


def load_step_api():
    sys.path.insert(0, str(OCP_CACHE))
    from OCP.BRepCheck import BRepCheck_Analyzer
    from OCP.IFSelect import IFSelect_RetDone
    from OCP.STEPControl import STEPControl_Reader
    return STEPControl_Reader, IFSelect_RetDone, BRepCheck_Analyzer


def validate_step(path, api):
    reader_cls, done, analyzer_cls = api
    reader = reader_cls()
    assert reader.ReadFile(str(path)) == done, path
    reader.TransferRoots()
    shape = reader.OneShape()
    assert not shape.IsNull() and analyzer_cls(shape).IsValid(), path


def parse_ascii_stl(path):
    vertices = []
    triangles = []
    current = []
    for line in path.read_text(encoding="ascii").splitlines():
        fields = line.split()
        if fields[:1] == ["vertex"]:
            point = tuple(float(v) for v in fields[1:4])
            vertices.append(point)
            current.append(point)
            if len(current) == 3:
                triangles.append(tuple(current))
                current = []
    assert triangles and not current, path
    quant = lambda p: tuple(round(v, 5) for v in p)
    edges = Counter()
    degenerate = 0
    for tri in triangles:
        a, b, c = tri
        ab = tuple(b[i] - a[i] for i in range(3))
        ac = tuple(c[i] - a[i] for i in range(3))
        cross = (
            ab[1] * ac[2] - ab[2] * ac[1],
            ab[2] * ac[0] - ab[0] * ac[2],
            ab[0] * ac[1] - ab[1] * ac[0],
        )
        if math.sqrt(sum(v * v for v in cross)) / 2 < 1e-9:
            degenerate += 1
        q = [quant(p) for p in tri]
        for i, j in ((0, 1), (1, 2), (2, 0)):
            edges[tuple(sorted((q[i], q[j])))] += 1
    xyz = list(zip(*vertices))
    bounds = [[min(axis), max(axis)] for axis in xyz]
    return {
        "triangles": len(triangles),
        "degenerate_triangles": degenerate,
        "boundary_edges": sum(v == 1 for v in edges.values()),
        "nonmanifold_edges": sum(v > 2 for v in edges.values()),
        "bounds_mm": bounds,
        "envelope_mm": [round(v[1] - v[0], 4) for v in bounds],
    }


def main():
    data = json.loads(MANIFEST.read_text(encoding="utf-8"))
    assert data["status"] == "candidate-only; dimensions-not-frozen"
    assert data["selection_frozen"] is False
    assert data["production_ready"] is False
    assert len(data["mandatory_measurements_before_freeze"]) >= 9
    assert set(data["shaft_interface_candidates"]) == {"round", "d-shaft", "knurled-adapter"}
    assert set(data["layouts"]) == {"a-balanced", "b-compact", "c-thumb-arc"}
    api = load_step_api()
    all_files = []
    for record in data["files"].values():
        all_files.extend(record.values())
    for layout in data["layouts"].values():
        assert set(layout["interface_candidates"]) == {"round", "d-shaft", "knurled-adapter"}
        for record in layout["interface_candidates"].values():
            all_files.extend(record["part_files"].values())
            all_files.extend(record["assembly_files"].values())
            assert all(abs(v) <= 1e-5 for v in record["hard_collision_common_volume_mm3"].values())
            assert min(record["screen_panel_clearance_mm"].values()) >= 3.0
    assert len(all_files) == len(set(all_files)), "manifest contains duplicate file paths"
    mesh = {}
    for relative in all_files:
        path = HERE / relative
        assert path.is_file() and path.stat().st_size > 100, path
        if path.suffix == ".step":
            validate_step(path, api)
        elif path.suffix == ".stl":
            stats = parse_ascii_stl(path)
            assert stats["degenerate_triangles"] == 0, (path, stats)
            assert stats["boundary_edges"] == 0, (path, stats)
            assert stats["nonmanifold_edges"] == 0, (path, stats)
            mesh[str(path.relative_to(HERE))] = stats
    output = {
        "status": "pass",
        "scope": "candidate geometry only; not proof of fit to the user's unmeasured EC12 or buttons",
        "selection_frozen": False,
        "production_ready": False,
        "layouts": 3,
        "shaft_interfaces_per_layout": 3,
        "step_files_checked": sum(p.endswith(".step") for p in all_files),
        "stl_files_checked": sum(p.endswith(".stl") for p in all_files),
        "mesh_checks": mesh,
    }
    (HERE / "MESH_AND_ASSEMBLY_REPORT.json").write_text(json.dumps(output, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    rows = []
    for layout_name, layout in data["layouts"].items():
        for interface, record in layout["interface_candidates"].items():
            collision_max = max(abs(v) for v in record["hard_collision_common_volume_mm3"].values())
            clearance_min = min(record["screen_panel_clearance_mm"].values())
            asm_stl = record["assembly_files"]["stl"]
            envelope = mesh[asm_stl]["envelope_mm"]
            rows.append(f"| {layout_name} | {interface} | {collision_max:.6f} | {clearance_min:.3f} | {envelope[0]:.3f} × {envelope[1]:.3f} × {envelope[2]:.3f} |")
    markdown = f"""# L2 可打印控制件验证报告

结果：**PASS（候选级，不代表已适配真实 EC12/轻触开关）**。

- 选择冻结：`false`
- 可用于生产：`false`
- STEP：{output['step_files_checked']} 个，全部可回读且 BRep 有效。
- STL：{output['stl_files_checked']} 个，全部无退化三角形、开边或非流形边。
- 布局：A balanced、B compact、C thumb-arc。
- 接口：round、D-shaft、knurled replaceable adapter；每套布局三种，共九套装配。
- 碰撞检查同时覆盖未按下状态、B/Menu 代理行程 0.35 mm 和 A 旋钮代理行程 0.50 mm。

| 布局 | 轴接口 | 最大硬碰撞体积/mm³ | 最小屏幕净距/mm | 组合 STL 包络/mm |
|---|---|---:|---:|---:|
{chr(10).join(rows)}

“0 碰撞”仅证明参数化代理在当前包络中不互穿。真实轴型、轴长、按压行程、开关执行头和打印收缩仍未测量，所以这些文件不是最终可直接打印件，OpenSpec 5.1/5.2/5.6/5.7 仍不得勾选。
"""
    (HERE / "VALIDATION_REPORT.md").write_text(markdown, encoding="utf-8")
    print(json.dumps({k: output[k] for k in ("status", "selection_frozen", "layouts", "shaft_interfaces_per_layout", "step_files_checked", "stl_files_checked")}))


if __name__ == "__main__":
    main()
