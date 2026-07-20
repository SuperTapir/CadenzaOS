# Rev A code-first automation

The source of truth is `design.yaml` plus `bom.yaml`. Both files currently use
JSON syntax, which is valid YAML 1.2 and keeps the bootstrap dependency-free.

Generate the interoperability smoke test:

```sh
python3 hardware/rev-a/scripts/generate.py
```

The command recreates `hardware/rev-a/generated/import-smoke-test/` with:

- a self-contained KiCad 5.x-compatible schematic, custom library, four-layer PCB and legacy project;
- a deterministic ZIP for JLCEDA Pro import;
- a 40 × 30 mm board-outline DXF;
- a D0 enclosure-envelope STL;
- BOM CSV, generated GPIO header, manifest and import checklist.

The smoke board intentionally contains only two named nets, two through-hole
connectors, two NPTH mounting holes, one routed segment and one ground zone. It
is not an electrical Rev A design and must never be sent for fabrication.

## Compatibility decision

The import passes only when JLCEDA Pro preserves the board dimensions, four
copper layers, net names, connector count, mounting holes and routed segment.
JLCEDA may rebuild the ground zone, but the result must remain assigned to GND
and must not create unrouted or shorted connections.

Save the successfully imported project as an `.epro` golden artifact and record
the editor/converter version and all warnings before expanding the generator to
the full Rev A schematic and enclosure.
