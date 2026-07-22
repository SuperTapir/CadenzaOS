#!/usr/bin/env python3
"""Generate a KiCad-owned donor schematic for the Cadenza L1 delta.

The installed ``kiutils`` release cannot safely rewrite the imported KiCad 10
schematic because it drops newer tokens.  It is still useful for creating an
isolated, old-format donor.  KiCad 10 then upgrades that donor and becomes the
authoritative writer of the symbol definitions and instances.  A separate
transplant step may copy only those generated blocks into the isolated L1
candidate; this script never edits the candidate or any reference source.
"""

from __future__ import annotations

import argparse
import copy
import subprocess
import uuid
from dataclasses import dataclass
from pathlib import Path

from kiutils.items.common import Effects, Font, PageSettings, Position, Property, TitleBlock
from kiutils.items.schitems import (
    Connection,
    GlobalLabel,
    SchematicSymbol,
    SymbolProjectInstance,
    SymbolProjectPath,
)
from kiutils.schematic import Schematic
from kiutils.symbol import Symbol, SymbolLib


KICAD_CLI = Path("/Applications/KiCad.app/Contents/MacOS/kicad-cli")
KICAD_SYMBOLS = Path("/Applications/KiCad.app/Contents/SharedSupport/symbols")
PROJECT_NAME = "cadenza-l1-symbol-donor"
UUID_NAMESPACE = uuid.UUID("f8633ea3-18a0-4bb6-9738-f2930646e77e")
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
    dnp: bool = False
    angle: int = 0


PARTS = [
    Part("Connector_Generic:Conn_01x10", "J_DISP1", "LS027B7DH01_10P_INTERFACE", 55, 65,
         {"1": "GPIO48", "2": "GPIO12", "3": "GPIO14", "4": "GPIO47", "5": "GPIO39",
          "6": "+5V", "7": "+5V", "8": "+5V", "9": "GND", "10": "GND"}),
    Part("Device:C", "C_DISP1", "100nF", 140, 65, {"1": "GPIO39", "2": "GND"},
         "Capacitor_SMD:C_0805_2012Metric", angle=90),
    Part("Device:C", "C_DISP2", "100nF", 205, 65, {"1": "+5V", "2": "GND"},
         "Capacitor_SMD:C_0805_2012Metric", angle=90),
    Part("Device:C", "C_DISP3", "1uF", 270, 65, {"1": "+5V", "2": "GND"},
         "Capacitor_SMD:C_0805_2012Metric", angle=90),
    Part("Diode:BAT54C", "D_PWR1", "BAT54C", 60, 175,
         {"1": "PWR_IP5306_KEY", "2": "PWR_SENSE_DIODE", "3": "PWR_BUTTON_NODE"},
         "Package_TO_SOT_SMD:SOT-23"),
    Part("Switch:SW_Push", "SW_PWR1", "NO_MOMENTARY_SIDE_PUSH", 185, 175,
         {"1": "PWR_BUTTON_NODE", "2": "GND"}),
    Part("Device:R", "R_PWR1", "1k", 60, 205,
         {"1": "PWR_SENSE_DIODE", "2": "PWR_KEY_SENSE"}, "Resistor_SMD:R_0805_2012Metric", angle=90),
    Part("Device:R", "R_PWR2", "10k", 185, 205,
         {"1": "3V3M", "2": "PWR_KEY_SENSE"}, "Resistor_SMD:R_0805_2012Metric", angle=90),
    Part("Transistor_FET:AO3400A", "Q_PWR1", "AO3400A", 60, 240,
         {"1": "PWR_GATE", "2": "GND", "3": "PWR_IP5306_KEY"}, "Package_TO_SOT_SMD:SOT-23"),
    Part("Device:R", "R_PWR3", "100R", 185, 240,
         {"1": "PWR_FORCE_OFF", "2": "PWR_GATE"}, "Resistor_SMD:R_0805_2012Metric", angle=90),
    Part("Device:R", "R_PWR4", "100k", 300, 240,
         {"1": "PWR_GATE", "2": "GND"}, "Resistor_SMD:R_0805_2012Metric", angle=90),
    Part("Device:R", "R_PWR5", "0R", 60, 260,
         {"1": "PWR_IP5306_KEY", "2": "PWR_BUTTON_NODE"}, "Resistor_SMD:R_0805_2012Metric", True, 90),
    Part("Device:R", "R_PWR6", "0R_HIGH_CURRENT_TBD", 220, 260,
         {"1": "/VOUT", "2": "+5V"}, "NetTie:NetTie-2_SMD_Pad2.0mm", angle=90),
]

TESTPOINT_NETS = {
    "TP_DISP_5V": "+5V", "TP_DISP_GND": "GND", "TP_DISP_SCLK": "GPIO48",
    "TP_DISP_SI": "GPIO12", "TP_DISP_SCS": "GPIO14", "TP_DISP_EXTCOMIN": "GPIO47",
    "TP_DISP_ENABLE": "GPIO39", "TP_PWR_KEY": "PWR_IP5306_KEY",
    "TP_PWR_BUTTON": "PWR_BUTTON_NODE", "TP_PWR_SENSE": "PWR_KEY_SENSE",
    "TP_PWR_FORCE": "PWR_FORCE_OFF", "TP_PWR_VOUT": "/VOUT", "TP_PWR_5V": "+5V",
}

TESTPOINT_POSITIONS = {
    "TP_DISP_5V": (55, 110), "TP_DISP_GND": (105, 110), "TP_DISP_SCLK": (155, 110),
    "TP_DISP_SI": (205, 110), "TP_DISP_SCS": (255, 110),
    "TP_DISP_EXTCOMIN": (305, 110), "TP_DISP_ENABLE": (355, 110),
    "TP_PWR_KEY": (330, 170), "TP_PWR_BUTTON": (380, 170), "TP_PWR_SENSE": (330, 205),
    "TP_PWR_FORCE": (380, 205), "TP_PWR_VOUT": (330, 250), "TP_PWR_5V": (380, 250),
}


def uid() -> str:
    global _uid_counter
    _uid_counter += 1
    return str(uuid.uuid5(UUID_NAMESPACE, f"{PROJECT_NAME}:{_uid_counter}"))


def load_symbol(lib_id: str) -> Symbol:
    nickname, name = lib_id.split(":", 1)
    library = SymbolLib.from_file(str(KICAD_SYMBOLS / f"{nickname}.kicad_sym"))
    symbols = {symbol.entryName: symbol for symbol in library.symbols}
    source = symbols[name]
    if source.extends:
        # Embedded schematic symbols are self-contained.  Flatten the one-level
        # AO3400A inheritance used here and retain the derived properties.
        base = copy.deepcopy(symbols[source.extends])
        base.properties = copy.deepcopy(source.properties)
        base.extends = None
        source = base
    else:
        source = copy.deepcopy(source)
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
        # KiCad libraries may put pins in the common unit (0) or the selected
        # unit (1).  SW_Push is a real example of the former.
        if unit.unitId not in (0, 1):
            continue
        for pin in unit.pins:
            pins[str(pin.number)] = pin.position
    return pins


def place(schematic: Schematic, library_symbol: Symbol, part: Part) -> None:
    # Keep all electrical endpoints on KiCad's 2.54 mm grid.  The imported
    # reference has legacy off-grid debt, but the L1 delta must not add more.
    origin_x = round(part.x / 2.54) * 2.54
    origin_y = round(part.y / 2.54) * 2.54
    placed = SchematicSymbol()
    placed.libId = part.lib_id
    placed.position = Position(X=origin_x, Y=origin_y, angle=part.angle)
    placed.unit = 1
    placed.inBom = True
    placed.onBoard = True
    placed.dnp = part.dnp
    placed.uuid = uid()
    reference_y = origin_y - (12 if part.reference == "J_DISP1" else 4)
    value_y = origin_y + (18 if part.reference == "J_DISP1" else 4)
    placed.properties = [
        property_("Reference", part.reference, origin_x, reference_y),
        property_("Value", part.value, origin_x, value_y, part.reference.startswith("TP_")),
        property_("Footprint", part.footprint, origin_x, origin_y, True),
        property_("Datasheet", "", origin_x, origin_y, True),
    ]
    pins = unit_pins(library_symbol)
    if set(pins) != set(part.pin_nets):
        raise RuntimeError(f"{part.reference}: pin set {sorted(pins)} != {sorted(part.pin_nets)}")
    for number, local in pins.items():
        placed.pins[number] = uid()
        angle = part.angle % 360
        if angle == 0:
            rx, ry = local.X, local.Y
        elif angle == 90:
            rx, ry = -local.Y, local.X
        elif angle == 180:
            rx, ry = -local.X, -local.Y
        elif angle == 270:
            rx, ry = local.Y, -local.X
        else:
            raise RuntimeError(f"unsupported placement angle {part.angle}")
        x = origin_x + rx
        y = origin_y - ry
        if part.reference.startswith("TP_"):
            label_x = x + 12.7
            label_angle = 0
        elif part.reference == "J_DISP1":
            label_x = x - 12.7
            label_angle = 180
        elif number == "1":
            label_x = x - 12.7
            label_angle = 180
        else:
            label_x = x + 12.7
            label_angle = 0
        schematic.graphicalItems.append(Connection(
            points=[Position(X=x, Y=y), Position(X=label_x, Y=y)],
            uuid=uid(),
        ))
        net = part.pin_nets[number]
        schematic.globalLabels.append(GlobalLabel(
            text=net,
            shape="bidirectional",
            position=Position(X=label_x, Y=y, angle=label_angle),
            effects=Effects(font=Font(width=1.27, height=1.27)),
            uuid=uid(),
        ))
    instance = SymbolProjectInstance(
        paths=[SymbolProjectPath(sheetInstancePath=f"/{schematic.uuid}", reference=part.reference, unit=1)],
    )
    instance.name = PROJECT_NAME
    placed.instances = [instance]
    schematic.schematicSymbols.append(placed)


def add_global_label(schematic: Schematic, text: str, x: float, y: float, angle: int = 0) -> None:
    schematic.globalLabels.append(GlobalLabel(
        text=text,
        shape="bidirectional",
        position=Position(X=x, Y=y, angle=angle),
        effects=Effects(font=Font(width=1.27, height=1.27)),
        uuid=uid(),
    ))


def build(output: Path) -> None:
    schematic = Schematic.create_new()
    schematic.uuid = uid()
    schematic.paper = PageSettings(paperSize="A3")
    schematic.titleBlock = TitleBlock(title="Cadenza L1 - Sharp Display and Power/Lock")

    required = sorted({part.lib_id for part in PARTS} | {"Connector:TestPoint"})
    libraries = {lib_id: load_symbol(lib_id) for lib_id in required}
    schematic.libSymbols.extend(libraries.values())
    for part in PARTS:
        place(schematic, libraries[part.lib_id], part)
    for reference, net in TESTPOINT_NETS.items():
        x, y = TESTPOINT_POSITIONS[reference]
        part = Part(
            "Connector:TestPoint", reference, "TESTPOINT", x, y, {"1": net},
            "TestPoint:TestPoint_Pad_D1.5mm",
        )
        place(schematic, libraries[part.lib_id], part)

    # Target-side anchors.  These carry no donor component endpoints, but
    # KiCad writes canonical global-label blocks that can be transplanted at
    # the exact reference schematic pin coordinates.
    add_global_label(schematic, "PWR_IP5306_KEY", 179.832, 30.226, 180)
    add_global_label(schematic, "PWR_KEY_SENSE", 346.710, 120.904, 0)
    add_global_label(schematic, "PWR_FORCE_OFF", 346.710, 108.204, 0)
    add_global_label(schematic, "GPIO12", 368.300, 142.494, 270)
    add_global_label(schematic, "GPIO14", 373.380, 142.494, 270)
    add_global_label(schematic, "GPIO47", 378.460, 142.494, 270)
    add_global_label(schematic, "GPIO48", 381.000, 142.494, 270)
    add_global_label(schematic, "GPIO39", 389.890, 115.824, 180)

    schematic.to_file(str(output), encoding="utf-8")
    subprocess.run([str(KICAD_CLI), "sch", "upgrade", "--force", str(output)], check=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    modern = args.output_dir / f"{PROJECT_NAME}.kicad_sch"
    build(modern)
    print(modern)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
