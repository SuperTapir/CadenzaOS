# S1 Power/Lock real-dimension fit candidate

Status: **non-frozen candidate**. `selection_frozen=false`, `production_ready=false`.

This replaces the old generic 6.6 mm switch-body proxy with a corrected GT-TC020A-H035-L1 page-1 envelope. Local X is the 4.6 mm width, local Y is front/back toward the enclosure edge, and local Z is height above PCB. The body is nominal X/Y/Z = 4.6/2.3/1.8 mm. `H035=3.50 mm` is total Y, **not Z height**. The actuator is 1.85 mm wide, 0.85 mm high, centred at CH=0.95 mm. The horizontal 0.66 and 0.85 side-view dimensions terminate at internal datums and are not used as travel or an invented external projection. Rest, nominal pressed (0.2 mm) and worst-case sweep (0.3 mm from 0.2±0.1 mm travel) are separate solids; movement is along Y only.

- `top-edge-A`: local outward +Y becomes global +Y; press is global -Y.
- `right-edge-B`: footprint rotates -90 degrees; local outward +Y becomes global +X; press is global -X.

Both placements retain the existing PCB outline and mounting holes. Zero collision only covers the L1 front shell, Sharp panel, FPC relief proxy, four mounting keepouts and all current L2 control proxies. It does not prove a complete enclosure, button cap, operating force or PCB assembly.

```sh
PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools-py313 /opt/homebrew/bin/python3.13 hardware/cadenza/mechanical/power-lock-s1-fit/generate_power_lock_s1_fit.py
PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools-py313 /opt/homebrew/bin/python3.13 hardware/cadenza/mechanical/power-lock-s1-fit/verify_power_lock_s1_fit.py
```
