#!/usr/bin/env python3
"""Apply only the frozen L1 screen replacement delta to the copied baseline."""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path


DELETE_REFS = {"FPC1", "U6", "Q2", "R13", "R14"}
DELETE_AUX_REFS = {"#PWR011", "#PWR016", "#PWR027", "#PWR045", "#PWR048"}
ADD_REFS = {"J_DISP1", "C_DISP1"}
DISPLAY_LABELS = {"GPIO12", "GPIO14", "GPIO39", "GPIO47", "GPIO48"}
OLD_LOCAL_LABELS = DISPLAY_LABELS | {"GPIO3"}
U4_GPIO3 = (355.600, 142.494)
DISPLAY_WIRE_UUIDS = {
    "45093a67-e046-451d-b5e2-8774711d19b6",
    "45a44e9f-68b9-4611-85f5-fe8ccb89d1cc",
    "cef4beb0-3921-4169-a849-2ec8f4b8a80b",
    "da8b5345-93b6-48ae-918d-527ef53cec82",
    "439c6424-1264-44ee-80b2-e8b20dd06d35",
    "0b9652df-2aee-4f2a-9514-7517d0035a26",
    "6aafbcc3-daa8-48ec-bdbb-bcc39d9d6f3a",
    "4f1293a0-32d9-4ec4-b398-9f4e5caca36d",
    "6f1d76eb-0cab-4bac-a55e-7a6da240f6aa",
    "529e2cbb-edba-47b2-9657-a7d9329603b3",
    "d4df2011-84cb-4fa2-a1b3-9dff669bcb1e",
    "e4458bc8-476a-4c80-8aad-69d9918ec624",
    "5a5ffd8c-e505-4329-8b3d-b879596fe9d8",
    "ed673115-e224-436e-bf2b-41c1abafe743",
}
DISPLAY_NC_UUIDS = {
    "07624c6c-0ae0-407a-8642-4a967acdef13",
    "3322be3c-2853-4730-8d82-5d74e3f8be7b",
    "2b0f966e-c29c-47f3-b1a4-618c07c2e7fa",
}
SHEET_UUID = "be19d0d1-f27a-5ee2-adaf-7fd049127297"
SHEET_FILENAME = "Cadenza-L1-Screen-Only-Display.kicad_sch"
ANCHOR_LABELS = {
    ("GPIO12", (368.300, 142.494)),
    ("GPIO14", (373.380, 142.494)),
    ("GPIO47", (378.460, 142.494)),
    ("GPIO48", (381.000, 142.494)),
    ("GPIO39", (389.890, 115.824)),
    ("+5V", (67.310, 191.008)),
}


@dataclass(frozen=True)
class Block:
    start: int
    end: int
    depth: int
    text: str


def blocks(text: str) -> list[Block]:
    stack: list[int] = []
    output: list[Block] = []
    quoted = False
    escaped = False
    for index, char in enumerate(text):
        if quoted:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                quoted = False
            continue
        if char == '"':
            quoted = True
        elif char == "(":
            stack.append(index)
        elif char == ")":
            if not stack:
                raise ValueError(f"unmatched ')' at {index}")
            start = stack.pop()
            output.append(Block(start, index + 1, len(stack) + 1, text[start:index + 1]))
    if stack:
        raise ValueError(f"unclosed '(' at {stack[-1]}")
    return output


def head(block: Block) -> str:
    match = re.match(r"\(\s*([^\s()]+)", block.text)
    return match.group(1) if match else ""


def reference(block: Block) -> str | None:
    match = re.search(r'\(property\s+"Reference"\s+"([^"]+)"', block.text)
    return match.group(1) if match else None


def at_xy(block: Block) -> tuple[float, float] | None:
    match = re.search(r"\(at\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)", block.text)
    return (float(match.group(1)), float(match.group(2))) if match else None


def block_uuid(block: Block) -> str | None:
    match = re.search(r'\(uuid\s+"?([^"\s()]+)"?\)', block.text)
    return match.group(1) if match else None


def label_name(block: Block) -> str | None:
    match = re.match(r'\((?:global_label|label)\s+"([^"]+)"', block.text)
    return match.group(1) if match else None


def coord_key(point: tuple[float, float] | None) -> tuple[float, float] | None:
    return None if point is None else (round(point[0], 3), round(point[1], 3))


def wire_touches(block: Block, point: tuple[float, float]) -> bool:
    coords = [(float(x), float(y)) for x, y in re.findall(
        r"\(xy\s+(-?\d+(?:\.\d+)?)\s+(-?\d+(?:\.\d+)?)\)", block.text
    )]
    return any(abs(x - point[0]) < 1e-6 and abs(y - point[1]) < 1e-6 for x, y in coords)


def remove_ranges(text: str, ranges: list[tuple[int, int]]) -> str:
    for start, end in sorted(ranges, reverse=True):
        text = text[:start] + text[end:]
    return text


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--donor", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--subsheet-output", type=Path, required=True)
    args = parser.parse_args()

    source = args.source.read_text(encoding="utf-8")
    donor = args.donor.read_text(encoding="utf-8")
    source_blocks = blocks(source)
    donor_blocks = blocks(donor)
    display_label_coords = {
        at_xy(block)
        for block in source_blocks
        if block.depth == 2 and head(block) == "label" and label_name(block) in DISPLAY_LABELS
    }
    if None in display_label_coords:
        raise RuntimeError("display label without coordinate")

    root_uuid_match = re.search(r'^\(kicad_sch.*?\(uuid\s+"?([^"\s()]+)"?\)', source, re.S)
    donor_uuid_match = re.search(r'^\(kicad_sch.*?\(uuid\s+"?([^"\s()]+)"?\)', donor, re.S)
    if not root_uuid_match or not donor_uuid_match:
        raise RuntimeError("root UUID not found")
    root_uuid, donor_uuid = root_uuid_match.group(1), donor_uuid_match.group(1)

    removals: list[tuple[int, int]] = []
    deleted: set[str] = set()
    deleted_aux: set[str] = set()
    removed_labels: list[str] = []
    removed_display_wires = 0
    removed_gpio3_wires = 0
    deleted_wire_uuids: set[str] = set()
    deleted_nc_uuids: set[str] = set()
    for block in source_blocks:
        if block.depth != 2:
            continue
        kind = head(block)
        if kind == "symbol":
            ref = reference(block)
            if ref in DELETE_REFS:
                removals.append((block.start, block.end)); deleted.add(ref)
            elif ref in DELETE_AUX_REFS:
                removals.append((block.start, block.end)); deleted_aux.add(ref)
        elif kind == "label" and label_name(block) in OLD_LOCAL_LABELS:
            removals.append((block.start, block.end)); removed_labels.append(label_name(block) or "")
        elif kind == "wire" and wire_touches(block, U4_GPIO3):
            removals.append((block.start, block.end)); removed_gpio3_wires += 1
        elif kind == "wire" and any(wire_touches(block, point) for point in display_label_coords):
            removals.append((block.start, block.end)); removed_display_wires += 1
        elif kind == "wire" and block_uuid(block) in DISPLAY_WIRE_UUIDS:
            removals.append((block.start, block.end)); deleted_wire_uuids.add(block_uuid(block) or "")
        elif kind == "no_connect" and block_uuid(block) in DISPLAY_NC_UUIDS:
            removals.append((block.start, block.end)); deleted_nc_uuids.add(block_uuid(block) or "")

    expected_label_counts = {name: 4 for name in OLD_LOCAL_LABELS}
    actual_label_counts = {name: removed_labels.count(name) for name in expected_label_counts}
    assertions = {
        "deleted refs": (deleted, DELETE_REFS),
        "deleted auxiliary refs": (deleted_aux, DELETE_AUX_REFS),
        "deleted fixed wires": (deleted_wire_uuids, DISPLAY_WIRE_UUIDS),
        "deleted no-connects": (deleted_nc_uuids, DISPLAY_NC_UUIDS),
        "local label counts": (actual_label_counts, expected_label_counts),
        "display label wires": (removed_display_wires, 14),
        "GPIO3 root wire": (removed_gpio3_wires, 1),
    }
    for name, (actual, expected) in assertions.items():
        if actual != expected:
            raise RuntimeError(f"{name}: expected {expected!r}, found {actual!r}")

    transformed = remove_ranges(source, removals)
    source_project_name = args.source.stem
    project_name = args.output.stem
    old_project_token = f'(project "{source_project_name}"'
    if old_project_token not in transformed:
        raise RuntimeError("source project instance metadata not found")
    transformed = transformed.replace(old_project_token, f'(project "{project_name}"')

    donor_symbols = {
        reference(block)
        for block in donor_blocks
        if block.depth == 2 and head(block) == "symbol" and reference(block) in ADD_REFS
    }
    if donor_symbols != ADD_REFS:
        raise RuntimeError(f"donor placed symbols mismatch: {sorted(donor_symbols)}")
    donor_labels = [
        block for block in donor_blocks
        if block.depth == 2 and head(block) in {"global_label", "label"}
    ]
    anchor_blocks = [
        block for block in donor_labels
        if (label_name(block), coord_key(at_xy(block))) in ANCHOR_LABELS
    ]
    attached_blocks = [block for block in donor_labels if block not in anchor_blocks]
    if len(anchor_blocks) != 6 or len(attached_blocks) != 12:
        raise RuntimeError(
            f"donor label split mismatch: anchors={len(anchor_blocks)} attached={len(attached_blocks)}"
        )

    gpio3_nc = ('(no_connect (at 355.6 142.494) '
                '(uuid "766153f7-635d-59a7-818c-a4dc021725ca"))')
    sheet = f'''(sheet (at 190 190) (size 80 30) (fields_autoplaced)
    (stroke (width 0) (type default))
    (fill (color 0 0 0 0.0000))
    (uuid "{SHEET_UUID}")
    (property "Sheetname" "L1 Sharp display only" (at 190 189.288 0)
      (effects (font (size 1.27 1.27)) (justify left bottom)))
    (property "Sheetfile" "{SHEET_FILENAME}" (at 190 220.66 0)
      (effects (font (size 1.27 1.27)) (justify left top)))
    (instances
      (project "{project_name}"
        (path "/{root_uuid}" (page "2")))))'''
    root_sheet_instances = f'''(sheet_instances
    (path "/{root_uuid}" (page "1")))'''
    payload = "\n\n" + gpio3_nc + "\n\n"
    payload += "\n\n".join(block.text for block in anchor_blocks)
    payload += "\n\n" + sheet + "\n\n" + root_sheet_instances + "\n"
    transformed = transformed[:-1] + payload + transformed[-1:]

    text_replacements = {
        "TFT_BLK - GPIO39": ("DISP - GPIO39", 2),
        "TFT_MOSI - GPIO12": ("DISP_SI - GPIO12", 2),
        "TFT_CLK - GPIO48": ("DISP_SCLK - GPIO48", 2),
        "TFT_DC - GPIO47": ("DISP_EXTCOMIN - GPIO47", 2),
        "TFT_RST - GPIO3\\n": ("", 2),
        "TFT_CS - GPIO14": ("DISP_SCS - GPIO14", 2),
        "ST7789LCD底座": ("Sharp LS027B7DH01 Memory LCD", 1),
    }
    for old, (new, expected_count) in text_replacements.items():
        actual_count = transformed.count(old)
        if actual_count != expected_count:
            raise RuntimeError(f"text replacement {old!r}: expected {expected_count}, found {actual_count}")
        transformed = transformed.replace(old, new)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(transformed, encoding="utf-8")
    subsheet = remove_ranges(donor, [(block.start, block.end) for block in anchor_blocks])
    subsheet = re.sub(r'\(project\s+"[^"]*"', f'(project "{project_name}"', subsheet)
    old_path = f'(path "/{donor_uuid}"'
    new_path = f'(path "/{root_uuid}/{SHEET_UUID}"'
    if old_path not in subsheet:
        raise RuntimeError("donor instance path not found")
    subsheet = subsheet.replace(old_path, new_path)
    args.subsheet_output.parent.mkdir(parents=True, exist_ok=True)
    args.subsheet_output.write_text(subsheet, encoding="utf-8")
    print(f"wrote {args.output}")
    print(f"wrote {args.subsheet_output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
