# JLCEDA Pro import result

Date: 2026-07-20  
JLCEDA Pro: V3.2.148  
Status: exchange path verified; design is not production-ready.

## Final verified project

- Project: `Cadenza Rev A Product D0 4L`
- Project ID: `ddf0de74fed044d0b32c68384ef2d08f`
- URL: https://pro.lceda.cn/editor#id=ddf0de74fed044d0b32c68384ef2d08f
- Imported archive SHA-256: `99e47a7cd89395b309fa755cf8e1635f4e81b8a146233c8b66b9c0be615b8f6b`

Confirmed in the native JLCEDA project:

- Board, schematic, and PCB documents were created.
- Copper stack contains Top, In1.Cu, In2.Cu, and Bottom.
- Main rectangular board outline and four NPTH mounting holes are visible.
- Fourteen named placement/interface blocks are visible.
- The imported ratlines preserve the named system connectivity.
- Display, RF keepout, speaker cavity, encoder daughterboard zones, and the
  `DO NOT FABRICATE` warning survived conversion.

Evidence: `generated/reports/jlceda-product-d0-4layer.png`.

## Enclosure exchange finding

JLCEDA Pro's documented enclosure workflow is native and one-way: draw the shell with
`Place > 3D Shell` primitives, then export upper/lower STL, STEP, or OBJ. The editor
does not expose an external STL-to-editable-shell import path. Consequently:

- `generated/enclosure.scad` remains the parameter source for the real enclosure.
- `mechanical/generated/*.stl` are D0 volume/fit exchange envelopes only.
- The final JLCEDA shell must be recreated from the same frozen dimensions using its
  native outline, pillar, datum-line, slot-region, and entity tools.
- Claiming that an externally imported STL is an editable JLCEDA shell would be false.

## Converter finding

The first product import (`d3f4f408c14c4bbea83e2709e66244ff`) exposed that inner
copper layers must be explicitly declared in the legacy KiCad layer table. That
project imported as two-layer and is superseded. The generator was corrected and
the final project above visibly includes both inner layers. The earlier smoke project
remains useful only as an importer sanity check.

## What this verification does not prove

- The interface-block footprints are not reviewed production land patterns.
- The PCB is intentionally unrouted; KiCad reports DRC/unconnected items.
- The encoder, speaker, battery, switch, and FPC contact side are not physically frozen.
- The SCAD and STL files are dimensional envelopes, not production mould/FDM models.
- JLC assembly stock and extended-part fees must be checked when the BOM is frozen.
- A final production import must be compared against Gerbers, drill files, assembly
  drawings, and the supplier 3D model before ordering.
