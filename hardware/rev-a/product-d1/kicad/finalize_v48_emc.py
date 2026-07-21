#!/usr/bin/env python3
"""Add conservative EMC return paths to the Rev-A production candidate."""

from pathlib import Path
from uuid import uuid4


ROOT = Path(__file__).resolve().parent
SOURCE = ROOT / "production-v42-priority" / "cadenza-d1-rev-a-production.kicad_pcb"
OUTPUT = ROOT / "production-v42-priority" / "cadenza-d1-rev-a-production-v48.kicad_pcb"


def uid() -> str:
    return str(uuid4())


def via(x: float, y: float) -> str:
    return f'''\t(via
\t\t(at {x:.4f} {y:.4f})
\t\t(size 0.6)
\t\t(drill 0.3)
\t\t(layers "F.Cu" "B.Cu")
\t\t(net "/GND")
\t\t(uuid "{uid()}")
\t)\n'''


def segment(x1: float, y1: float, x2: float, y2: float, width: float = 0.5) -> str:
    return f'''\t(segment
\t\t(start {x1:.4f} {y1:.4f})
\t\t(end {x2:.4f} {y2:.4f})
\t\t(width {width:.2f})
\t\t(layer "B.Cu")
\t\t(net "/GND")
\t\t(uuid "{uid()}")
\t)\n'''


text = SOURCE.read_text()

# Match the schematic labels exactly; the electrical net names remain UART_TX0/RX0.
text = text.replace('(property "Value" "UART_TX0"', '(property "Value" "UART_TX"', 1)
text = text.replace('(property "Value" "UART_RX0"', '(property "Value" "UART_RX"', 1)

# The original 1.2 mm VBAT diagonal crossed directly under U8 pad 2, making a
# low-inductance ESD ground via impossible. Preserve its endpoints and width,
# but dogleg around the ESD device on In2.Cu.
old_vbat = '''\t(segment
\t\t(start 96.2825 83.4837)
\t\t(end 117.4417 104.6429)
\t\t(width 1.2)
\t\t(locked yes)
\t\t(layer "In2.Cu")
\t\t(net "/VBAT")
\t\t(uuid "864d8125-fa3d-4aa3-a72a-1136fdc88f32")
\t)'''
new_vbat = f'''\t(segment
\t\t(start 96.2825 83.4837)
\t\t(end 112.0000 99.2000)
\t\t(width 1.2)
\t\t(locked yes)
\t\t(layer "In2.Cu")
\t\t(net "/VBAT")
\t\t(uuid "{uid()}")
\t)
\t(segment
\t\t(start 112.0000 99.2000)
\t\t(end 115.5000 99.2000)
\t\t(width 1.2)
\t\t(locked yes)
\t\t(layer "In2.Cu")
\t\t(net "/VBAT")
\t\t(uuid "{uid()}")
\t)
\t(segment
\t\t(start 115.5000 99.2000)
\t\t(end 117.4417 104.6429)
\t\t(width 1.2)
\t\t(locked yes)
\t\t(layer "In2.Cu")
\t\t(net "/VBAT")
\t\t(uuid "{uid()}")
\t)'''
if old_vbat not in text:
    raise RuntimeError("Original VBAT segment not found")
text = text.replace(old_vbat, new_vbat, 1)

# U8 USBLC6-2SC6 pad 2 is at (114.8625, 101.0000). Keep the ESD discharge
# path short and wide. A tented production via overlaps only the outer edge of
# the pad, avoiding the VBAT route to the left and VBUS route under the body.
additions = via(114.35, 101.0)

# Insert board-level copper immediately before the final board close.
end = text.rfind("\n)")
if end < 0:
    raise RuntimeError("KiCad board closing parenthesis not found")
text = text[:end] + "\n" + additions + text[end:]
OUTPUT.write_text(text)
print(OUTPUT)
