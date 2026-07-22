#!/usr/bin/env python3
"""Verify the L1 screen-only candidate against fresh KiCad XML netlists."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

from transplant_screen_only_delta import blocks, head, reference


EXPECTED_BASELINE_SHA256 = "1d3ee26e1fd7aee38fa1636f32753dfe49b6c6bf6b1f5c19aefac4dae8223a7c"
DELETED = {"FPC1", "U6", "Q2", "R13", "R14"}
ADDED = {"J_DISP1", "C_DISP1"}
SHARP_PIN_MAP = {
    "1": "GPIO39", "2": "GPIO48", "3": "GPIO47", "4": "GPIO14", "5": "GPIO12",
    "6": "+5V", "7": "+5V", "8": "+5V", "9": "GND", "10": "GND",
}
J_DISP1_FOOTPRINT = (
    "Cadenza_Display_FPC_Variants:"
    "Hirose_FH34SRJ-10S-0.5SH_50_1x10_P0.50mm_DualContact"
)


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def canonical_net(name: str) -> str | None:
    if name.startswith("unconnected-"):
        return None
    return name[1:] if name.startswith("/") else name


def parse_netlist(path: Path) -> tuple[dict[str, dict], dict[str, dict[str, str | None]], dict[str, set[str]]]:
    root = ET.parse(path).getroot()
    components: dict[str, dict] = {}
    for comp in root.findall("./components/comp"):
        ref = comp.get("ref") or ""
        components[ref] = {
            "value": comp.findtext("value") or "",
            "footprint": comp.findtext("footprint") or "",
            "datasheet": comp.findtext("datasheet") or "",
            "description": comp.findtext("description") or "",
            "fields": {field.get("name") or "": field.text or "" for field in comp.findall("./fields/field")},
            "libsource": dict((comp.find("libsource").attrib if comp.find("libsource") is not None else {})),
        }
    pins: dict[str, dict[str, str | None]] = {ref: {} for ref in components}
    nets: dict[str, set[str]] = {}
    for net in root.findall("./nets/net"):
        name = canonical_net(net.get("name") or "")
        if name is None:
            for node in net.findall("node"):
                pins.setdefault(node.get("ref") or "", {})[node.get("pin") or ""] = None
            continue
        endpoints = nets.setdefault(name, set())
        for node in net.findall("node"):
            ref, pin = node.get("ref") or "", node.get("pin") or ""
            endpoints.add(f"{ref}.{pin}")
            pins.setdefault(ref, {})[pin] = name
    return components, pins, nets


def placed_symbols(path: Path) -> dict[str, str]:
    text = path.read_text(encoding="utf-8")
    found = {}
    for block in blocks(text):
        if block.depth == 2 and head(block) == "symbol":
            ref = reference(block)
            if ref and not ref.startswith("#"):
                found[ref] = re.sub(r'\(project\s+"[^"]+"', '(project "<PROJECT>"', block.text)
    return found


def check(checks: list[dict], check_id: str, expected, actual) -> None:
    checks.append({
        "id": check_id,
        "status": "PASS" if actual == expected else "FAIL",
        "expected": expected,
        "actual": actual,
    })


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline-schematic", type=Path, required=True)
    parser.add_argument("--candidate-schematic", type=Path, required=True)
    parser.add_argument("--baseline-netlist", type=Path, required=True)
    parser.add_argument("--candidate-netlist", type=Path, required=True)
    parser.add_argument("--output-json", type=Path, required=True)
    parser.add_argument("--output-md", type=Path, required=True)
    args = parser.parse_args()

    base_hash_before = sha256(args.baseline_schematic)
    base_components, base_pins, base_nets = parse_netlist(args.baseline_netlist)
    candidate_components, candidate_pins, candidate_nets = parse_netlist(args.candidate_netlist)
    base_symbols = placed_symbols(args.baseline_schematic)
    candidate_symbols = placed_symbols(args.candidate_schematic)
    checks: list[dict] = []

    check(checks, "baseline_hash_matches_frozen_copy", EXPECTED_BASELINE_SHA256, base_hash_before)
    check(
        checks,
        "designator_delta_exact",
        {"deleted": sorted(DELETED), "added": sorted(ADDED)},
        {
            "deleted": sorted(set(base_components) - set(candidate_components)),
            "added": sorted(set(candidate_components) - set(base_components)),
        },
    )
    check(checks, "candidate_real_component_count", len(base_components) - 5 + 2, len(candidate_components))

    retained = sorted(set(base_components) - DELETED)
    metadata_drift = [ref for ref in retained if base_components[ref] != candidate_components.get(ref)]
    check(checks, "retained_component_metadata_unchanged", [], metadata_drift)
    raw_symbol_drift = [ref for ref in retained if base_symbols[ref] != candidate_symbols.get(ref)]
    check(checks, "retained_placed_symbol_blocks_unchanged", [], raw_symbol_drift)
    check(
        checks,
        "C20_C21_value_footprint_and_properties_preserved",
        {ref: base_components[ref] for ref in ("C20", "C21")},
        {ref: candidate_components.get(ref) for ref in ("C20", "C21")},
    )

    sharp_actual = candidate_pins.get("J_DISP1", {})
    check(checks, "sharp_pin_map_exact", SHARP_PIN_MAP, sharp_actual)
    check(checks, "J_DISP1_selected_footprint_exact", J_DISP1_FOOTPRINT, candidate_components.get("J_DISP1", {}).get("footprint"))
    check(checks, "J_DISP1_value_exact", "FH34SRJ-10S-0.5SH(50)", candidate_components.get("J_DISP1", {}).get("value"))
    check(checks, "C_DISP1_footprint_reuses_reference_0805", "Cadenza-re-easyedapro:C0805", candidate_components.get("C_DISP1", {}).get("footprint"))
    check(checks, "C_DISP1_value", "100nF", candidate_components.get("C_DISP1", {}).get("value"))
    check(checks, "C_DISP1_pin_map", {"1": "GPIO12", "2": "GND"}, candidate_pins.get("C_DISP1", {}))
    check(checks, "C20_power_pin_only_changed_to_5V", {"1": "GND", "2": "+5V"}, candidate_pins.get("C20", {}))
    check(checks, "C21_power_pin_only_changed_to_5V", {"1": "+5V", "2": "GND"}, candidate_pins.get("C21", {}))
    check(checks, "U4_pin15_explicitly_unused", None, candidate_pins.get("U4", {}).get("15"))

    allowed_retained_changes = {("C20", "2"), ("C21", "1"), ("U4", "15")}
    endpoint_drift = []
    for ref in retained:
        all_pins = set(base_pins.get(ref, {})) | set(candidate_pins.get(ref, {}))
        for pin in sorted(all_pins):
            if (ref, pin) in allowed_retained_changes:
                continue
            before, after = base_pins.get(ref, {}).get(pin), candidate_pins.get(ref, {}).get(pin)
            if before != after:
                endpoint_drift.append({"endpoint": f"{ref}.{pin}", "before": before, "after": after})
    check(checks, "all_other_retained_endpoints_unchanged", [], endpoint_drift)

    forbidden_nets = {
        "3V3LCD", "GPIO3", "Net-(Q2-B)", "Net-(Q2-C)",
        "unconnected-(FPC1-Pad6)", "unconnected-(FPC1-Pad13)", "unconnected-(FPC1-Pad14)",
    }
    present_forbidden = sorted(name for name in forbidden_nets if name in candidate_nets)
    check(checks, "old_display_nets_removed", [], present_forbidden)

    forbidden_new_refs = sorted(
        ref for ref in candidate_components
        if ref.startswith("TP") or ref.startswith("R_PWR") or ref.startswith("SW_PWR")
        or ref.startswith("Q_PWR") or ref.startswith("D_PWR")
    )
    check(checks, "no_testpoint_or_power_lock_additions", [], forbidden_new_refs)

    protected_refs = {"USB1", "U3", "Q1", "SW1", "KEY1", "KEY2", "KEY3", "KEY4", "KEY5", "KEY6", "BOOT1", "RST1"}
    protected_drift = []
    for ref in sorted(protected_refs):
        if base_components.get(ref) != candidate_components.get(ref) or base_pins.get(ref) != candidate_pins.get(ref):
            protected_drift.append(ref)
    check(checks, "USB_power_and_buttons_unchanged", [], protected_drift)

    base_hash_after = sha256(args.baseline_schematic)
    check(checks, "baseline_not_modified_during_generation", base_hash_before, base_hash_after)
    failed = [item["id"] for item in checks if item["status"] != "PASS"]
    report = {
        "schema_version": 1,
        "gate": "cadenza_l1_screen_only_schematic_boundary",
        "status": "PASS_CANDIDATE" if not failed else "FAIL",
        "production_ready": False,
        "fpc_selection_frozen": True,
        "pcb_connector_rotation_frozen": False,
        "evidence_scope": "screen_only_schematic_and_kicad_xml_netlist",
        "baseline": {
            "path": str(args.baseline_schematic),
            "sha256_before": base_hash_before,
            "sha256_after": base_hash_after,
            "unchanged": base_hash_before == base_hash_after,
        },
        "candidate": {
            "path": str(args.candidate_schematic),
            "sha256": sha256(args.candidate_schematic),
            "netlist": str(args.candidate_netlist),
            "component_count": len(candidate_components),
        },
        "summary": {"total": len(checks), "passed": len(checks) - len(failed), "failed": len(failed)},
        "failed_checks": failed,
        "checks": checks,
        "limits": [
            "J_DISP1 part and footprint are selected, but PCB rotation and the final Pin-1 physical orientation are not frozen.",
            "No PCB was generated or modified; routing, placement, DRC, fabrication outputs, and hardware bring-up remain outside this evidence.",
        ],
    }
    args.output_json.parent.mkdir(parents=True, exist_ok=True)
    args.output_json.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    rows = "\n".join(f"| `{item['id']}` | {item['status']} |" for item in checks)
    markdown = f"""# L1 Screen-only 原理图边界验证

- 状态：**{report['status']}**
- `production_ready=false`
- `fpc_selection_frozen=true`
- `pcb_connector_rotation_frozen=false`
- 基线 SHA-256（构建前/后）：`{base_hash_before}` / `{base_hash_after}`
- 候选：`{args.candidate_schematic}`
- KiCad XML netlist：`{args.candidate_netlist}`
- 结果：{report['summary']['passed']}/{report['summary']['total']} checks passed

| 检查 | 结果 |
|---|---|
{rows}

## 证据边界

本报告只证明 XML netlist 和原理图对象满足冻结的“只换屏”差异边界。J_DISP1 型号和
footprint 已选定，但 PCB 旋转角度与最终 Pin 1 实物方向尚未冻结；未生成或修改 PCB，未覆盖布局、布线、
DRC、制造文件或真机点屏。因此该候选不是生产就绪设计。
"""
    args.output_md.write_text(markdown, encoding="utf-8")
    print(json.dumps({"status": report["status"], "failed_checks": failed}, ensure_ascii=False))
    return 0 if not failed else 1


if __name__ == "__main__":
    raise SystemExit(main())
