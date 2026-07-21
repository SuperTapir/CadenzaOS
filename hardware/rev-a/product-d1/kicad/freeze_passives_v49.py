#!/usr/bin/env python3
"""Freeze high-stock Rev-A resistor/capacitor parts in the KiCad source."""

from pathlib import Path
import re
import shutil


ROOT = Path(__file__).resolve().parent
SCH = ROOT / "cadenza-d1.kicad_sch"
PCB = ROOT / "production-v42-priority" / "cadenza-d1-rev-a-production.kicad_pcb"

# Exact LCSC catalogue matches checked on 2026-07-21. Power magnetics, the
# current shunt, and mechanically sensitive parts are intentionally excluded.
GROUPS = {
    ("CL10A106KP8NNNC", "C19702"): "C1 C2 C3 C10 C11 C20 C50 C81 C90".split(),
    ("CL10A226MP8NUNE", "C86295"): "C12 C13 C21 C22".split(),
    ("CL10B104KA8NNNC", "C1590"): "C14 C31 C40 C51 C82 C91".split(),
    ("CL10B105KO8NNNC", "C59782"): "C30 C70 C71 C80".split(),
    ("0603WAF5101T5E", "C23186"): "R1 R2".split(),
    ("0603WAF1501T5E", "C22843"): "R3 R5".split(),
    ("0603WAF1201T5E", "C22765"): ["R4"],
    ("0603WAF4702T5E", "C25819"): ["R6"],
    ("0603WAF1002T5E", "C25804"): "R7 R72 R74 R80 R90 R91 R92 R93".split(),
    ("0603WAF4703T5E", "C23178"): ["R10"],
    ("0603WAF1503T5E", "C22807"): ["R11"],
    ("FRC0603F7323TS", "C5159703"): ["R20"],
    ("0603WAF1003T5E", "C25803"): "R21 R30 R41 R75 R76 R77".split(),
    ("0603WAF1000T5E", "C22775"): ["R40"],
    ("0603WAF4701T5E", "C23162"): "R70 R71".split(),
}


def balanced_symbol_ranges(text: str) -> list[tuple[int, int]]:
    stack: list[tuple[int, bool]] = []
    ranges: list[tuple[int, int]] = []
    quoted = escaped = False
    i = 0
    while i < len(text):
        c = text[i]
        if quoted:
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == '"':
                quoted = False
        elif c == '"':
            quoted = True
        elif c == "(":
            stack.append((i, text.startswith("(symbol", i)))
        elif c == ")" and stack:
            start, is_symbol = stack.pop()
            if is_symbol:
                ranges.append((start, i + 1))
        i += 1
    return ranges


def update_symbol(block: str, ref: str, mpn: str, lcsc: str) -> str:
    if ref == "R4":
        block = block.replace('(property "Value" "1.18k"', '(property "Value" "1.2k"', 1)
    block, n1 = re.subn(r'\(property "MPN" "[^"]*"', f'(property "MPN" "{mpn}"', block, count=1)
    block, n2 = re.subn(r'\(property "LCSC" "[^"]*"', f'(property "LCSC" "{lcsc}"', block, count=1)
    if n1 != 1 or n2 != 1:
        raise RuntimeError(f"Missing MPN/LCSC property for {ref}")
    return block


text = SCH.read_text()
ranges = balanced_symbol_ranges(text)
ref_to_part = {ref: part for part, refs in GROUPS.items() for ref in refs}
edits: list[tuple[int, int, str]] = []
for ref, (mpn, lcsc) in ref_to_part.items():
    needle = f'(property "Reference" "{ref}"'
    pos = text.find(needle)
    if pos < 0:
        raise RuntimeError(f"Schematic reference not found: {ref}")
    candidates = [(a, b) for a, b in ranges if a <= pos < b]
    if not candidates:
        raise RuntimeError(f"Owning symbol not found: {ref}")
    a, b = min(candidates, key=lambda pair: pair[1] - pair[0])
    edits.append((a, b, update_symbol(text[a:b], ref, mpn, lcsc)))

for a, b, replacement in sorted(edits, reverse=True):
    text = text[:a] + replacement + text[b:]

sch_backup = SCH.with_name("cadenza-d1.pre-passive-freeze-v49.kicad_sch")
pcb_backup = PCB.with_name("cadenza-d1-rev-a-production.pre-passive-freeze-v49.kicad_pcb")
if not sch_backup.exists():
    shutil.copy2(SCH, sch_backup)
if not pcb_backup.exists():
    shutil.copy2(PCB, pcb_backup)
SCH.write_text(text)

pcb_text = PCB.read_text()
old = '(property "Value" "1.18k"'
if pcb_text.count(old) != 1:
    raise RuntimeError("Expected exactly one R4 value in production PCB")
PCB.write_text(pcb_text.replace(old, '(property "Value" "1.2k"', 1))
print(f"Updated {len(ref_to_part)} schematic symbols and R4 PCB value")
