#!/usr/bin/env python3
"""Generate the two-symbol, screen-only KiCad donor schematic.

The imported KiCad 10 baseline is not rewritten through kiutils.  This script
uses kiutils only to create an isolated donor, then asks KiCad 10 to upgrade it.
The transplant script copies the donor-owned sheet into the candidate.
"""

from __future__ import annotations

import argparse
import copy
import subprocess
import uuid
from dataclasses import dataclass
from pathlib import Path

from kiutils.items.common import Effects, Font, PageSettings, Position, Property, TitleBlock
from kiutils.items.schitems import Connection, GlobalLabel, SchematicSymbol, SymbolProjectInstance, SymbolProjectPath
from kiutils.schematic import Schematic
from kiutils.symbol import Symbol, SymbolLib


KICAD_CLI = Path("/Applications/KiCad.app/Contents/MacOS/kicad-cli")
KICAD_SYMBOLS = Path("/Applications/KiCad.app/Contents/SharedSupport/symbols")
PROJECT_NAME = "Cadenza-reference-ESP32-S3-game-console-original-v2-20260722"
UUID_NAMESPACE = uuid.UUID("ccf20c90-02d8-43c5-b0e5-02eb121f8c1e")
_uid_counter = 0


@dataclass(frozen=True)
class Part:
    lib_id: str
    reference: str
    value: str
    x: float
    y: float
    pin_nets: dict[str, str]
    footprint: str = ""
    angle: int = 0
    properties: tuple[tuple[str, str], ...] = ()


PARTS = [
    Part(
        "Connector_Generic:Conn_01x10",
        "J_DISP1",
        "FH34SRJ-10S-0.5SH(50)",
        55,
        65,
        {
            "1": "GPIO39",
            "2": "GPIO48",
            "3": "GPIO47",
            "4": "GPIO14",
            "5": "GPIO12",
            "6": "+5V",
            "7": "+5V",
            "8": "+5V",
            "9": "GND",
            "10": "GND",
        },
        "Cadenza_Display_FPC_Variants:Hirose_FH34SRJ-10S-0.5SH_50_1x10_P0.50mm_DualContact",
        0,
        (
            ("Manufacturer Part", "FH34SRJ-10S-0.5SH(50)"),
            ("Manufacturer", "Hirose Electric"),
            ("Supplier Part", "C324723"),
            ("Supplier", "LCSC"),
            ("JLCPCB Part Class", "Extended Part"),
            ("Display Module", "Sharp LS027B7DH01"),
            ("Contact Side", "Top and bottom"),
            ("FPC", "10P, 0.5 mm pitch, 0.30 mm thick"),
        ),
    ),
    Part(
        "Device:C",
        "C_DISP1",
        "100nF",
        140,
        65,
        {"1": "GPIO12", "2": "GND"},
        "Cadenza-re-easyedapro:C0805",
        90,
        (
            ("Manufacturer Part", "FCC0805B104K500DT"),
            ("Manufacturer", "FOJAN(富捷)"),
            ("Supplier Part", "C5137467"),
            ("Supplier", "LCSC"),
            ("JLCPCB Part Class", "Extended Part"),
            ("Tolerance", "±10%"),
            ("Voltage Rating", "50V"),
            ("Temperature Coefficient", "X7R"),
        ),
    ),
]

ROOT_ANCHORS = [
    ("GPIO12", 368.300, 142.494, 270),
    ("GPIO14", 373.380, 142.494, 270),
    ("GPIO47", 378.460, 142.494, 270),
    ("GPIO48", 381.000, 142.494, 270),
    ("GPIO39", 389.890, 115.824, 180),
    ("+5V", 67.310, 191.008, 0),
]


def uid() -> str:
    global _uid_counter
    _uid_counter += 1
    return str(uuid.uuid5(UUID_NAMESPACE, f"screen-only:{_uid_counter}"))


def load_symbol(lib_id: str) -> Symbol:
    nickname, name = lib_id.split(":", 1)
    library = SymbolLib.from_file(str(KICAD_SYMBOLS / f"{nickname}.kicad_sym"))
    symbols = {symbol.entryName: symbol for symbol in library.symbols}
    source = copy.deepcopy(symbols[name])
    source.libId = lib_id
    return source


def property_(key: str, value: str, x: float, y: float, hide: bool = False) -> Property:
    return Property(
        key=key,
        value=value,
        position=Position(X=x, Y=y, angle=0),
        effects=Effects(font=Font(width=1.27, height=1.27), hide=hide),
    )


def unit_pins(symbol: Symbol) -> dict[str, Position]:
    pins: dict[str, Position] = {}
    for unit in symbol.units:
        if unit.unitId not in (0, 1):
            continue
        for pin in unit.pins:
            pins[str(pin.number)] = pin.position
    return pins


def place(schematic: Schematic, library_symbol: Symbol, part: Part) -> None:
    origin_x = round(part.x / 2.54) * 2.54
    origin_y = round(part.y / 2.54) * 2.54
    placed = SchematicSymbol()
    placed.libId = part.lib_id
    placed.position = Position(X=origin_x, Y=origin_y, angle=part.angle)
    placed.unit = 1
    placed.inBom = True
    placed.onBoard = True
    placed.uuid = uid()
    placed.properties = [
        property_("Reference", part.reference, origin_x, origin_y - (12 if part.reference == "J_DISP1" else 4)),
        property_("Value", part.value, origin_x, origin_y + (18 if part.reference == "J_DISP1" else 4)),
        property_("Footprint", part.footprint, origin_x, origin_y, True),
        property_("Datasheet", "", origin_x, origin_y, True),
    ]
    placed.properties.extend(
        property_(key, value, origin_x, origin_y, True) for key, value in part.properties
    )
    pins = unit_pins(library_symbol)
    if set(pins) != set(part.pin_nets):
        raise RuntimeError(f"{part.reference}: pin set {sorted(pins)} != {sorted(part.pin_nets)}")
    for number, local in pins.items():
        placed.pins[number] = uid()
        if part.angle == 0:
            rx, ry = local.X, local.Y
        elif part.angle == 90:
            rx, ry = -local.Y, local.X
        else:
            raise RuntimeError(f"unsupported placement angle {part.angle}")
        x, y = origin_x + rx, origin_y - ry
        if part.reference == "J_DISP1" or number == "1":
            label_x, label_angle = x - 12.7, 180
        else:
            label_x, label_angle = x + 12.7, 0
        schematic.graphicalItems.append(
            Connection(points=[Position(X=x, Y=y), Position(X=label_x, Y=y)], uuid=uid())
        )
        schematic.globalLabels.append(
            GlobalLabel(
                text=part.pin_nets[number],
                shape="bidirectional",
                position=Position(X=label_x, Y=y, angle=label_angle),
                effects=Effects(font=Font(width=1.27, height=1.27)),
                uuid=uid(),
            )
        )
    placed.instances = [
        SymbolProjectInstance(
            name=PROJECT_NAME,
            paths=[SymbolProjectPath(sheetInstancePath=f"/{schematic.uuid}", reference=part.reference, unit=1)],
        )
    ]
    schematic.schematicSymbols.append(placed)


def add_global_label(schematic: Schematic, text: str, x: float, y: float, angle: int) -> None:
    schematic.globalLabels.append(
        GlobalLabel(
            text=text,
            shape="bidirectional",
            position=Position(X=x, Y=y, angle=angle),
            effects=Effects(font=Font(width=1.27, height=1.27)),
            uuid=uid(),
        )
    )


def build(output: Path) -> None:
    schematic = Schematic.create_new()
    schematic.uuid = uid()
    schematic.paper = PageSettings(paperSize="A3")
    schematic.titleBlock = TitleBlock(title="Cadenza L1 - Sharp display only")
    libraries = {lib_id: load_symbol(lib_id) for lib_id in sorted({p.lib_id for p in PARTS})}
    schematic.libSymbols.extend(libraries.values())
    for part in PARTS:
        place(schematic, libraries[part.lib_id], part)
    for anchor in ROOT_ANCHORS:
        add_global_label(schematic, *anchor)
    schematic.to_file(str(output), encoding="utf-8")
    subprocess.run([str(KICAD_CLI), "sch", "upgrade", "--force", str(output)], check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    build(args.output)
    print(args.output)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
