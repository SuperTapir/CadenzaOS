#!/usr/bin/env python3
"""Add fabrication-identifying silkscreen to the Rev-A production board."""

from pathlib import Path
from uuid import uuid4


ROOT = Path(__file__).resolve().parent
PCB = ROOT / "production-v42-priority" / "cadenza-d1-rev-a-production.kicad_pcb"


def text_item(label: str, x: float, y: float) -> str:
    return f'''\t(gr_text "{label}"
\t\t(at {x:.3f} {y:.3f} 0)
\t\t(layer "F.SilkS")
\t\t(uuid "{uuid4()}")
\t\t(effects
\t\t\t(font
\t\t\t\t(size 1 1)
\t\t\t\t(thickness 0.15)
\t\t\t)
\t\t\t(justify left bottom)
\t\t)
\t)\n'''


source = PCB.read_text()
if 'CADENZA D1 REV A' in source:
    raise RuntimeError("Release marking already present")
marking = text_item("CADENZA D1 REV A", 52.5, 112.5)
marking += text_item("HW V50 2026-07", 58.0, 110.9)
end = source.rfind("\n)")
if end < 0:
    raise RuntimeError("KiCad board closing parenthesis not found")
PCB.write_text(source[:end] + "\n" + marking + source[end:])
print(PCB)
