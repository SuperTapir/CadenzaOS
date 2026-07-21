# Cadenza D1 Encoder + A Daughterboard

This 34 mm x 38 mm, two-layer daughterboard implements the right-side primary
control as one replaceable assembly:

- ALPS Alpine EC12D1524403 incremental encoder, 30 detents / 15 pulses
- axial encoder push switch mapped to `A_SW`
- local 10 kOhm / 10 nF debounce networks for A, B, and A_SW
- reserved `LED_CTRL` line for a future illuminated-knob revision
- JST-SH 1.0 mm six-pin cable to the main board
- four M2 mounting holes and a 28 mm maximum knob envelope

## Frozen interface

| J1 pin | Net | Direction at daughterboard |
| --- | --- | --- |
| 1 | +3V3 | input |
| 2 | GND | return |
| 3 | ENC_A | output, active low |
| 4 | ENC_B | output, active low |
| 5 | A_SW | output, active low |
| 6 | LED_CTRL / RESERVED | no-connect on D1 daughterboard |

The cable pin order must match main-board J6 exactly. Do not reverse the cable
at assembly time; pin 1 is marked on both boards.

## Mechanical stack

- PCB: 34.0 x 38.0 x 1.6 mm, nominal.
- Encoder shaft axis: X=17.0 mm, Y=17.0 mm from the board upper-left datum.
- Knob: 28.0 mm maximum OD, 6.0 mm D-shaft bore, axial movement at least
  0.5 mm, and 0.6 mm radial clearance to the front-shell aperture.
- Front shell must react encoder bushing load; the PCB screws are alignment and
  anti-rotation features, not the sole push-load path.
- J1 is on the back side at the lower edge. Reserve 8 mm cable bend clearance.
- M2 holes are centered 3 mm from each board corner. Use M2 x 4 heat-set inserts
  in the front shell and M2 x 5 screws from the rear.

## Manufacturing intent

The encoder is through-hole and should be hand/secondary soldered after SMT
assembly. J1 and the debounce passives are JLCPCB-assemblable. Import the generated
`cadenza-encoder-a.kicad_pcb` directly into EasyEDA Pro, then verify pin 1,
board outline, plated slots, and the encoder courtyard against this document.

Regenerate with:

```sh
python3 generate.py
```
