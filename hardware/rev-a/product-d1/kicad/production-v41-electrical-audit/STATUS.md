# Cadenza D1 PCB v41 status

## Manufacturing status

**DO NOT FABRICATE OR ASSEMBLE v41 YET.**

The electrical schematic audit is complete and the canonical schematic passes
ERC with 0 errors and 0 warnings. The PCB electrical migration is correct, but
the present component placement cannot yet be routed to production quality.

## Trusted artifacts

- `../cadenza-d1.kicad_sch`: canonical modern KiCad schematic; ERC 0/0.
- `cadenza-d1-v41.kicad_pcb`: clean v41 PCB baseline with corrected nets,
  current-sense path, pull-ups, decoupling, test points, and fiducials.
- `drc-v41-pre-route.rpt`: baseline DRC; no shorts, crossings, clearance,
  board-edge, courtyard, or electrical errors. Unrouted nets are expected.
- `cadenza-d1-v41-routed.kicad_pcb`: best routing experiment. It has no
  shorts, crossings, clearance, width, drill, or board-edge errors, but still
  has 22 unconnected reports: five functional routing gaps plus disconnected
  GND pour islands.
- `drc-v41-routed.rpt`: DRC evidence for the routing experiment.

## Rejected artifacts

Files containing `.rejected.` or `.manual-failed.` are diagnostic evidence
only. They contain DRC failures and must never be uploaded for fabrication.

## Placement blockers discovered by routing

1. The LCD keepout blocks the shortest legal `LCD_SCS` escape; the J2-to-U1
   route must go around the display perimeter, but the current U1 pad field has
   no safe transition-via location.
2. U5 has no legal escape channel for `I2S_BCLK` without crossing `I2S_DIN`,
   `AMP_EN`, or adjacent NC/power pads.
3. U7, R72, R7, and U2 are packed too tightly for `GAUGE_BIN` and `BAT_TS` at
   the configured clearance and drill rules.
4. The U3/L2/U2 power area leaves `VSYS` split into multiple islands once the
   required current-path widths and clearances are honored.
5. Front, rear, and inner GND pours need intentional stitching-via placement;
   random vias in the current dense area collide with signal tracks and pads.

## Required v42 work

1. Re-place U1, U5, U7/R72, R7/U2, and U3/L2 to create explicit escape
   channels before any autorouting.
2. Reserve the inner GND layer as an uninterrupted reference plane. Keep
   ordinary signals off it.
3. Route and lock current paths, USB, LCD control, I2S clocks, battery sense,
   and gauge Kelvin/sense nets first.
4. Add GND stitching vias near layer transitions and around the board
   perimeter, then refill all zones.
5. Autoroute only the remaining low-risk GPIO/control nets.
6. Require ERC 0/0, DRC 0 errors, zero unconnected items, Gerber inspection,
   BOM/CPL validation, and a successful EasyEDA Pro import before release.

