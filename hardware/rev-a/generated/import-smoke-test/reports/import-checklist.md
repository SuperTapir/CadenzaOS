# JLCEDA Pro import smoke-test checklist

Import `cadenza_rev_a_smoke-jlc-import.zip` as a KiCad project.

Expected authoritative values:

- Board: 40.0 × 30.0 mm rectangle
- Copper layers: 4 (`F.Cu`, `In1.Cu`, `In2.Cu`, `B.Cu`)
- Nets: `GND`, `TEST_SIG`
- Connectors: 2 (`J1`, `J2`), each 2-pin through-hole
- NPTH mounting holes: 2, diameter 3.2 mm
- Routed segments: 2: `TEST_SIG` on `F.Cu` and `GND` on `B.Cu`
- Copper zones: 1 `GND` zone on `B.Cu`; JLCEDA is expected to rebuild it
- Board text: `CADENZA REV A IMPORT SMOKE`

Record import version, warnings, observed values, copper rebuild result, and whether an exported `.epro` can be reopened.
