#!/usr/bin/env python3
"""Transplant the validated L1 donor delta into an isolated KiCad schematic.

This is a deliberately narrow textual transformer.  It preserves every byte
outside explicitly selected top-level S-expression blocks, avoiding the KiCad
10 token loss seen when the current kiutils release rewrites a whole imported
schematic.  KiCad CLI is the parser and netlist authority after the transform.
"""

from __future__ import annotations

import argparse
import re
from dataclasses import dataclass
from pathlib import Path


DELETE_REFS = {"FPC1", "U6", "C20", "C21", "Q2", "R13", "R14", "SW1", "KEY1"}
DELETE_AUX_REFS = {"#PWR004", "#PWR011", "#PWR016", "#PWR027", "#PWR030", "#PWR045", "#PWR048"}
ADD_REFS = {
    "J_DISP1", "C_DISP1", "C_DISP2", "C_DISP3",
    "TP_DISP_5V", "TP_DISP_GND", "TP_DISP_SCLK", "TP_DISP_SI",
    "TP_DISP_SCS", "TP_DISP_EXTCOMIN", "TP_DISP_ENABLE",
    "D_PWR1", "SW_PWR1", "R_PWR1", "R_PWR2", "Q_PWR1", "R_PWR3",
    "R_PWR4", "R_PWR5", "R_PWR6", "TP_PWR_KEY", "TP_PWR_BUTTON",
    "TP_PWR_SENSE", "TP_PWR_FORCE", "TP_PWR_VOUT", "TP_PWR_5V",
}
ADD_LIB_IDS = {
    "Connector_Generic:Conn_01x10", "Device:C", "Diode:BAT54C",
    "Switch:SW_Push", "Device:R", "Transistor_FET:AO3400A",
    "Connector:TestPoint",
}
DISPLAY_LABELS = {"GPIO12", "GPIO14", "GPIO39", "GPIO47", "GPIO48"}
OLD_LOCAL_LABELS = {"GPIO18", "GPIO6", "GPIO3"} | DISPLAY_LABELS
SW2_PAD5 = (229.616, 153.924)
U4_GPIO3 = (355.600, 142.494)
U1_KEY_NC = (179.832, 30.226)
DELETE_WIRE_UUIDS = {
    "45093a67-e046-451d-b5e2-8774711d19b6", "45a44e9f-68b9-4611-85f5-fe8ccb89d1cc",
    "cef4beb0-3921-4169-a849-2ec8f4b8a80b", "6aafbcc3-daa8-48ec-bdbb-bcc39d9d6f3a",
    "3b4cf75a-dab5-47a1-99ea-067462872c23", "2249af1c-6875-47ef-924b-941512d52349",
    "6f1d76eb-0cab-4bac-a55e-7a6da240f6aa", "4f1293a0-32d9-4ec4-b398-9f4e5caca36d",
    "3a7cadf8-b453-4c53-97d7-775e2601863a",
    "da8b5345-93b6-48ae-918d-527ef53cec82", "439c6424-1264-44ee-80b2-e8b20dd06d35",
    "9ebcb989-cbfb-4ba9-8de0-5a68011a9a0b", "529e2cbb-edba-47b2-9657-a7d9329603b3",
    "d4df2011-84cb-4fa2-a1b3-9dff669bcb1e", "e4458bc8-476a-4c80-8aad-69d9918ec624",
    "5a5ffd8c-e505-4329-8b3d-b879596fe9d8", "ed673115-e224-436e-bf2b-41c1abafe743",
    "0b9652df-2aee-4f2a-9514-7517d0035a26",
}
DELETE_NC_UUIDS = {
    "07624c6c-0ae0-407a-8642-4a967acdef13", "3322be3c-2853-4730-8d82-5d74e3f8be7b",
    "2b0f966e-c29c-47f3-b1a4-618c07c2e7fa", "d86aeaf2-b06f-4825-8692-686ce681170b",
    "ab9d0a64-2c22-4222-8bee-7aa3f02dcc0f", "e2b07a45-8af1-42ea-a735-f7493d5ae39c",
}
SHEET_UUID = "4ca2b48d-38de-5fc3-8c48-68da268a2403"
SHEET_FILENAME = "Cadenza-L1-Sharp-Power.kicad_sch"
ANCHOR_LABELS = {
    ("PWR_IP5306_KEY", (179.832, 30.226)),
    ("PWR_KEY_SENSE", (346.710, 120.904)),
    ("PWR_FORCE_OFF", (346.710, 108.204)),
    ("GPIO12", (368.300, 142.494)),
    ("GPIO14", (373.380, 142.494)),
    ("GPIO47", (378.460, 142.494)),
    ("GPIO48", (381.000, 142.494)),
    ("GPIO39", (389.890, 115.824)),
}
FOOTPRINT_METADATA_REPAIRS = {
    "H1": (
        "Cadenza-re-easyedapro:",
        "Cadenza-re-easyedapro:CONN-SMD_2P-P2.00_XUNPU_WAFER-PH2.0-2PWB",
    ),
    "Q1": (
        "Cadenza-re-easyedapro:",
        "Cadenza-re-easyedapro:SC-70-6_L2.2-W1.3-P0.65-LS2.1-BR",
    ),
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


def lib_id(block: Block) -> str | None:
    match = re.match(r'\(symbol\s+"([^"]+)"', block.text)
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


def repair_footprint_metadata(text: str, reference_name: str, old: str, new: str) -> str:
    matches = [
        block for block in blocks(text)
        if block.depth == 2 and head(block) == "symbol" and reference(block) == reference_name
    ]
    if len(matches) != 1:
        raise RuntimeError(f"expected one placed symbol for {reference_name}, found {len(matches)}")
    block = matches[0]
    old_token = f'(property "Footprint" "{old}"'
    new_token = f'(property "Footprint" "{new}"'
    if block.text.count(old_token) != 1:
        raise RuntimeError(f"expected one {reference_name} footprint token {old!r}")
    replacement = block.text.replace(old_token, new_token)
    return text[:block.start] + replacement + text[block.end:]


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
        if block.depth == 2
        and head(block) == "label"
        and (match := re.match(r'\(label\s+"([^"]+)"', block.text))
        and match.group(1) in DISPLAY_LABELS
    }
    if None in display_label_coords:
        raise RuntimeError("display label without coordinate")

    root_uuid_match = re.search(r'^\(kicad_sch.*?\(uuid\s+"?([^"\s()]+)"?\)', source, re.S)
    donor_uuid_match = re.search(r'^\(kicad_sch.*?\(uuid\s+"?([^"\s()]+)"?\)', donor, re.S)
    if not root_uuid_match or not donor_uuid_match:
        raise RuntimeError("root UUID not found")
    root_uuid = root_uuid_match.group(1)
    donor_uuid = donor_uuid_match.group(1)
    source_project_name = args.source.stem
    project_name = args.output.stem

    removals: list[tuple[int, int]] = []
    deleted: set[str] = set()
    removed_labels: list[str] = []
    removed_sw2_wires = 0
    removed_gpio3_wires = 0
    removed_display_wires = 0
    removed_u1_nc = 0
    deleted_aux: set[str] = set()
    deleted_wire_uuids: set[str] = set()
    deleted_nc_uuids: set[str] = set()
    for block in source_blocks:
        if block.depth != 2:
            continue
        kind = head(block)
        if kind == "symbol":
            ref = reference(block)
            if ref in DELETE_REFS:
                removals.append((block.start, block.end))
                deleted.add(ref)
            elif ref in DELETE_AUX_REFS:
                removals.append((block.start, block.end))
                deleted_aux.add(ref)
        elif kind == "label":
            match = re.match(r'\(label\s+"([^"]+)"', block.text)
            if match and match.group(1) in OLD_LOCAL_LABELS:
                removals.append((block.start, block.end))
                removed_labels.append(match.group(1))
        elif kind == "wire" and wire_touches(block, SW2_PAD5):
            removals.append((block.start, block.end))
            removed_sw2_wires += 1
        elif kind == "wire" and wire_touches(block, U4_GPIO3):
            removals.append((block.start, block.end))
            removed_gpio3_wires += 1
        elif kind == "wire" and any(wire_touches(block, point) for point in display_label_coords):
            removals.append((block.start, block.end))
            removed_display_wires += 1
        elif kind == "wire" and block_uuid(block) in DELETE_WIRE_UUIDS:
            removals.append((block.start, block.end))
            deleted_wire_uuids.add(block_uuid(block))
        elif kind == "no_connect" and block_uuid(block) in DELETE_NC_UUIDS:
            removals.append((block.start, block.end))
            deleted_nc_uuids.add(block_uuid(block))
        elif kind == "no_connect" and at_xy(block) == U1_KEY_NC:
            removals.append((block.start, block.end))
            removed_u1_nc += 1

    if deleted != DELETE_REFS:
        raise RuntimeError(f"delete refs mismatch: found {sorted(deleted)}")
    if deleted_aux != DELETE_AUX_REFS:
        raise RuntimeError(f"delete auxiliary refs mismatch: found {sorted(deleted_aux)}")
    if deleted_wire_uuids != DELETE_WIRE_UUIDS:
        raise RuntimeError(f"delete wire UUID mismatch: found {sorted(deleted_wire_uuids)}")
    if deleted_nc_uuids != DELETE_NC_UUIDS:
        raise RuntimeError(f"delete no-connect UUID mismatch: found {sorted(deleted_nc_uuids)}")
    expected_label_counts = {
        "GPIO18": 3, "GPIO3": 4, "GPIO6": 3,
        "GPIO12": 4, "GPIO14": 4, "GPIO39": 4, "GPIO47": 4, "GPIO48": 4,
    }
    actual_label_counts = {name: removed_labels.count(name) for name in expected_label_counts}
    if actual_label_counts != expected_label_counts:
        raise RuntimeError(f"unexpected GPIO6/GPIO18 labels: {sorted(removed_labels)}")
    if removed_sw2_wires != 1:
        raise RuntimeError(f"expected one SW2 pad-5 wire, found {removed_sw2_wires}")
    if removed_gpio3_wires != 1:
        raise RuntimeError(f"expected one U4 GPIO3 wire, found {removed_gpio3_wires}")
    if removed_display_wires != 14:
        raise RuntimeError(f"expected 14 old display wires, found {removed_display_wires}")
    if removed_u1_nc != 1:
        raise RuntimeError(f"expected one U1 KEY no-connect, found {removed_u1_nc}")

    transformed = remove_ranges(source, removals)
    for ref, (old_footprint, new_footprint) in FOOTPRINT_METADATA_REPAIRS.items():
        transformed = repair_footprint_metadata(
            transformed, ref, old_footprint, new_footprint
        )
    old_project_token = f'(project "{source_project_name}"'
    new_project_token = f'(project "{project_name}"'
    if old_project_token not in transformed:
        raise RuntimeError("source project instance metadata not found")
    transformed = transformed.replace(old_project_token, new_project_token)

    donor_symbols: set[str] = set()
    donor_labels: list[Block] = []
    for block in donor_blocks:
        if block.depth != 2:
            continue
        if head(block) == "symbol":
            ref = reference(block)
            if ref in ADD_REFS:
                donor_symbols.add(ref)
        elif head(block) in {"global_label", "label"}:
            donor_labels.append(block)
    if set(donor_symbols) != ADD_REFS:
        raise RuntimeError(f"donor placed symbols mismatch: {sorted(donor_symbols)}")

    anchor_blocks = [
        block for block in donor_labels
        if (label_name(block), coord_key(at_xy(block))) in ANCHOR_LABELS
    ]
    attached_blocks = [block for block in donor_labels if block not in anchor_blocks]
    if len(anchor_blocks) != len(ANCHOR_LABELS) or len(attached_blocks) != 49:
        raise RuntimeError(
            f"donor label split mismatch: anchors={len(anchor_blocks)} attached={len(attached_blocks)}"
        )

    sw2_nc = ('(no_connect (at 229.616 153.924) '
               '(uuid "71c5f778-dd47-5549-a1c3-9c344601d7cf"))')
    gpio3_nc = ('(no_connect (at 355.6 142.494) '
                '(uuid "2708a1e0-b7a1-50a3-95f4-6675ab8f75e0"))')
    sheet = f'''(sheet (at 190 190) (size 80 30) (fields_autoplaced)
    (stroke (width 0) (type default))
    (fill (color 0 0 0 0.0000))
    (uuid "{SHEET_UUID}")
    (property "Sheetname" "L1 Sharp + Power/Lock" (at 190 189.288 0)
      (effects (font (size 1.27 1.27)) (justify left bottom)))
    (property "Sheetfile" "{SHEET_FILENAME}" (at 190 220.66 0)
      (effects (font (size 1.27 1.27)) (justify left top)))
    (instances
      (project "{project_name}"
        (path "/{root_uuid}" (page "2")))))'''
    root_sheet_instances = f'''(sheet_instances
    (path "/{root_uuid}" (page "1")))'''
    payload = "\n\n" + sw2_nc + "\n\n" + gpio3_nc + "\n\n"
    payload += "\n\n".join(block.text for block in anchor_blocks)
    payload += "\n\n" + sheet + "\n\n" + root_sheet_instances + "\n"
    transformed = transformed[:-1] + payload + transformed[-1:]

    text_replacements = {
        "TFT_BLK - GPIO39": ("DISP_ENABLE - GPIO39", 2),
        "TFT_MOSI - GPIO12": ("DISP_SI - GPIO12", 2),
        "TFT_CLK - GPIO48": ("DISP_SCLK - GPIO48", 2),
        "TFT_DC - GPIO47": ("DISP_EXTCOMIN - GPIO47", 2),
        "TFT_RST - GPIO3\\n": ("", 2),
        "TFT_CS - GPIO14": ("DISP_SCS - GPIO14", 2),
        "KEY_MENU - GPIO18": ("PWR_KEY_SENSE - GPIO18", 2),
        "KEY_RIGHT - GPIO6": ("PWR_FORCE_OFF - GPIO6", 2),
        "ST7789LCD底座": ("Sharp LS027B7DH01 Memory LCD", 1),
    }
    for old, (new, expected_count) in text_replacements.items():
        actual_count = transformed.count(old)
        if actual_count != expected_count:
            raise RuntimeError(f"text replacement {old!r}: expected {expected_count}, found {actual_count}")
        transformed = transformed.replace(old, new)
    old_title_anchor = '(text "Sharp LS027B7DH01 Memory LCD" (exclude_from_sim no) (at 238.76 214.884 0)'
    new_title_anchor = '(text "Sharp LS027B7DH01 Memory LCD" (exclude_from_sim no) (at 20 180 0)'
    if transformed.count(old_title_anchor) != 1:
        raise RuntimeError("Sharp title anchor not found exactly once")
    transformed = transformed.replace(old_title_anchor, new_title_anchor)

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
    print(
        f"deleted={len(deleted)} added={len(donor_symbols)} "
        f"root_anchors={len(anchor_blocks)} subsheet_labels={len(attached_blocks)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
