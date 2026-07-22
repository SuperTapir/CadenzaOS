# GT-TC020A-H035-L1 Power/Lock footprint candidate

Status: **candidate only; `selection_frozen=false`; `production_ready=false`**.

The sole dimensional authority is manufacturer drawing page 1 in `C17179533_GT-TC020A-H035-L1.pdf`. Local axes are explicit: X is the 4.6 mm width, Y is front/back toward the enclosure edge, and Z is height above PCB. The body is nominal X/Y/Z = 4.6/2.3/1.8 mm. `H035=3.50 mm` is the total Y depth from rear datum to actuator tip, **not Z height**. The actuator is 1.85 mm wide, 0.85 mm high and centred at CH=0.95 mm above the PCB. The horizontal side-view 0.66 and 0.85 dimensions end at internal/contact datums; they are not interpreted as travel or a unique external stem projection.

The recommended PCB layout gives two 0.6 x 0.94 mm electrical lands, a 1.54 x 0.94 mm unnumbered mechanical solder land, two Ø0.70 mm holes on 1.70 mm centres and two Ø0.77 mm holes on 4.35 mm centres, all at ±0.05 mm layout tolerance. Pin 1 is left and pin 2 right when read in the manufacturer recommended-layout view. The local footprint is mirrored in Y only so the actuator points toward local +Y; pressing and the 0.20±0.10 mm travel are local -Y.

The PDF does not name a copper keepout. Therefore this candidate does not invent one: maximum body/actuator limits are shown on Fab and Courtyard. The central hatched land is unnumbered because the page-1 circuit diagram contains only terminals 1 and 2.

The STEP/STL is a simple nominal envelope generated from page-1 dimensions, not a vendor production model.

```sh
PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools-py313 \
  /opt/homebrew/bin/python3.13 \
  hardware/cadenza/libraries/power-lock-switch-candidate/generate_candidate_model.py

/Applications/KiCad.app/Contents/MacOS/kicad-cli fp export svg \
  --layers F.Cu,F.Paste,F.Mask,F.SilkS,F.Fab,F.CrtYd \
  --output hardware/cadenza/libraries/power-lock-switch-candidate/render \
  hardware/cadenza/libraries/power-lock-switch-candidate/GT_TC020A_H035_L1.pretty

/opt/homebrew/bin/python3.13 \
  hardware/cadenza/libraries/power-lock-switch-candidate/verify_candidate.py
```
