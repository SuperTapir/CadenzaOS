#!/usr/bin/env python3
"""Read-only preflight for syncing the L1 schematic into the inherited PCB."""

from __future__ import annotations

import argparse
import json
import xml.etree.ElementTree as ET
from pathlib import Path

import pcbnew


PHYSICAL_SELECTION_BLOCKERS = {"J_DISP1", "SW_PWR1"}
PCB_IMPLEMENTATION_PENDING: set[str] = set()
EXPECTED_IMPORT_METADATA_REPAIRS = {
    "H1": "Cadenza-re-easyedapro:CONN-SMD_2P-P2.00_XUNPU_WAFER-PH2.0-2PWB",
    "Q1": "Cadenza-re-easyedapro:SC-70-6_L2.2-W1.3-P0.65-LS2.1-BR",
}


def xml_components(path: Path) -> dict[str, dict[str, str]]:
    root = ET.parse(path).getroot()
    return {
        node.attrib["ref"]: {
            "value": node.findtext("value", default=""),
            "footprint": node.findtext("footprint", default="").strip(),
        }
        for node in root.findall("./components/comp")
        if not node.attrib["ref"].startswith("#")
    }


def board_footprints(path: Path) -> dict[str, str]:
    board = pcbnew.LoadBoard(str(path))
    result: dict[str, str] = {}
    for footprint in board.GetFootprints():
        fpid = footprint.GetFPID()
        nickname = str(fpid.GetLibNickname())
        item = str(fpid.GetLibItemName())
        result[footprint.GetReference()] = f"{nickname}:{item}" if nickname else item
    return result


def references(items: list) -> set[str]:
    return {item["reference"] if isinstance(item, dict) else item for item in items}


def has_footprint(value: str) -> bool:
    return bool(value and not value.endswith(":"))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--board", type=Path, required=True)
    parser.add_argument("--netlist", type=Path, required=True)
    parser.add_argument("--contract", type=Path, required=True)
    parser.add_argument("--json-output", type=Path)
    parser.add_argument("--markdown-output", type=Path)
    args = parser.parse_args()

    contract = json.loads(args.contract.read_text(encoding="utf-8"))
    candidate = xml_components(args.netlist)
    board = board_footprints(args.board)
    deleted = references(contract["reference_components"]["delete"])
    modified = references(contract["reference_components"]["modify"])
    retained = references(contract["reference_components"]["retain"])
    reference = deleted | modified | retained
    new = {item["reference"] for item in contract["new_components"]}
    target = (reference - deleted) | new

    retained_or_modified = retained | modified
    missing_retained = sorted(
        ref for ref in retained_or_modified if not has_footprint(candidate[ref]["footprint"])
    )
    retained_mismatches = sorted([
        {
            "reference": ref,
            "schematic": candidate[ref]["footprint"],
            "board": board[ref],
        }
        for ref in retained_or_modified
        if has_footprint(candidate[ref]["footprint"]) and candidate[ref]["footprint"] != board[ref]
    ], key=lambda item: item["reference"])
    new_missing = sorted(ref for ref in new if not has_footprint(candidate[ref]["footprint"]))
    new_ready = sorted(new - set(new_missing))
    expected_new_missing = PHYSICAL_SELECTION_BLOCKERS | PCB_IMPLEMENTATION_PENDING

    checks = {
        "reference_board_has_exact_77_refs": set(board) == reference and len(board) == 77,
        "candidate_has_exact_94_real_refs": set(candidate) == target and len(candidate) == 94,
        "deleted_refs_are_the_only_reference_removals": deleted == set(board) - set(candidate),
        "retained_nonempty_footprints_match_board": not retained_mismatches,
        "no_retained_footprints_are_missing": not missing_retained,
        "exact_import_metadata_repairs_match_board": all(
            candidate[ref]["footprint"] == expected and board[ref] == expected
            for ref, expected in EXPECTED_IMPORT_METADATA_REPAIRS.items()
        ),
        "new_missing_footprints_are_exactly_declared_pending": set(new_missing) == expected_new_missing,
        "physical_selection_blockers_remain_unfrozen": PHYSICAL_SELECTION_BLOCKERS <= set(new_missing),
    }
    result = {
        "schema_version": 1,
        "status": "PASS_BLOCKED_BY_DECLARED_FOOTPRINTS" if all(checks.values()) else "FAIL",
        "production_ready": False,
        "pcb_sync_ready": False,
        "checks": checks,
        "counts": {
            "reference_board_footprints": len(board),
            "candidate_real_components": len(candidate),
            "deleted_reference_footprints": len(deleted),
            "new_components": len(new),
            "new_components_with_footprints": len(new_ready),
            "new_components_without_footprints": len(new_missing),
        },
        "blockers": {
            "physical_selection": sorted(PHYSICAL_SELECTION_BLOCKERS),
            "pcb_implementation": sorted(PCB_IMPLEMENTATION_PENDING),
        },
        "completed_metadata_repairs": [
            {"reference": ref, "recovered_from_board": board[ref]}
            for ref in sorted(EXPECTED_IMPORT_METADATA_REPAIRS)
        ],
        "ready_new_components": new_ready,
        "new_missing_footprints": new_missing,
        "retained_footprint_mismatches": retained_mismatches,
        "interpretation": [
            "The inherited PCB is a valid 77-footprint placement source, not an L1 PCB candidate.",
            "Ten new L1 components already have ordinary KiCad footprints.",
            "J_DISP1 and SW_PWR1 require physical part selection before footprint freeze.",
            "R_PWR6 and thirteen test points now have reviewed PCB-only KiCad footprints.",
            "H1 and Q1 imported footprint metadata has been recovered exactly from the protected board.",
        ],
    }
    rendered = json.dumps(result, ensure_ascii=False, indent=2) + "\n"
    if args.json_output:
        args.json_output.parent.mkdir(parents=True, exist_ok=True)
        args.json_output.write_text(rendered, encoding="utf-8")
    if args.markdown_output:
        blockers = "\n".join(
            f"- `{row['reference']}`：从参考 PCB 恢复 `{row['recovered_from_board']}`。"
            for row in result["completed_metadata_repairs"]
        )
        args.markdown_output.write_text(
            f"""# L1 PCB 同步前置检查

状态：**{result['status']}**。这不是 PCB 同步通过，也不是可生产结论。

- 参考 PCB：{len(board)} 个 footprint。
- 正式候选原理图：{len(candidate)} 个实体器件。
- 新增 26 个器件中，{len(new_ready)} 个已有 footprint，{len(new_missing)} 个仍需实物选型。
- `pcb_sync_ready=false`，`production_ready=false`。

## 必须等实物

- `J_DISP1`：真实 FPC 触点面、Pin 1、锁扣和出线方向。
- `SW_PWR1`：真实侧按开关料号、焊盘、按压方向和外壳推杆配合。

## 可以先做但尚未实施

- `R_PWR6` 已指定 `NetTie-2_SMD_Pad2.0mm` 短宽铜桥候选；仍须在 PCB 上检查电流回路和铜宽。
- 13 个测试点已指定 `TestPoint_Pad_D1.5mm` 裸铜焊盘；最终 BOM/CPL 必须排除。
- 已修复两项导入缺失的 footprint 元数据：
{blockers}

## 边界

参考 PCB 仍只是 77-footprint 的布局来源。本报告没有创建或同步 L1 PCB。
""",
            encoding="utf-8",
        )
    print(rendered, end="")
    return 0 if result["status"].startswith("PASS") else 1


if __name__ == "__main__":
    raise SystemExit(main())
