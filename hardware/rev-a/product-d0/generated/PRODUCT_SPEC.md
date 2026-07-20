# Cadenza Rev A Product D0

> Status: engineering draft. The files in `generated/` are intended for EDA import,
> review, enclosure fit checks, and supplier discussion. Do not fabricate until every
> item in the release gate has been signed off.

## 1. Product intent

Cadenza is a pocketable, landscape, always-readable audio computer. It keeps the
T-Embed interaction economy while replacing the parts that currently constrain the
experience: the LCD, microphone, loudspeaker, battery system, and fixed enclosure.

The front face is deliberately asymmetric:

```text
     front, 122 x 74 mm nominal
    /----------------------------------------------------------\
   /   o MIC      +----------------------------------+          \
  |  [MENU]       |                                  |   .----.  |
  |               |  Sharp LS027B7DH01, 400 x 240    |  /  A   \ |
  |    [ B ]      |                                  | | encoder| |
  |  :::::::::    +----------------------------------+  \______/ |
   \ speaker                                                   /
    \----------------------------------------------------------/
```

- Left: `Menu` above `B`, microphone port, front speaker grille.
- Centre: Sharp 2.7-inch Memory LCD in landscape orientation.
- Right: an annular rotary encoder and its centre push action, named `A`.
- Top/side: latching power switch and USB-C.
- Rear: four screws, battery service panel, acoustic cavity, two desk-rest facets.

The faceted rear shell provides nominal 18 degree and 35 degree desk angles without
adding a kickstand. The enclosure remains comfortable in two hands; the facets are
behind the grip line rather than sharp front-edge decoration.

## 2. Decisions and sourcing policy

Only the display is immutable. Every other part follows this priority:

1. JLCPCB/LCSC basic or extended SMT part with stable manufacturer part number.
2. Same-footprint second source, selected by assembly BOM substitution.
3. Hand-installed commodity module or mechanical part from Taobao.
4. A replaceable daughterboard when mechanics and feel dominate availability.

The right control therefore lives on `ENCODER_DB`: a 6-wire daughterboard
(`3V3`, `GND`, `ENC_A`, `ENC_B`, `A_SW`, `LED`). Its hole pattern and wheel are not
frozen until physical samples have been measured. Changing encoder vendor must not
change the main PCB.

## 3. Electrical architecture

| Block | D0 choice | Freeze class | Notes |
|---|---|---|---|
| MCU/radio | ESP32-S3-WROOM-1-N16R8, LCSC C2913202 | frozen | 16 MB flash, 8 MB PSRAM, native Wi-Fi/BLE and USB |
| Display | LS027B7DH01 | frozen | 10-pin 0.5 mm FPC, 5 V panel supply, 3.3 V logic |
| Display FPC | FH34SRJ-10S-0.5SH(50), C324723 | conditional | verify contact side against the physical screen before release |
| 3.3 V rail | AP2112K-3.3 | replaceable | D0 limit; replace with >=1 A buck-boost if measured peaks fail |
| Display 5 V | TPS61023 adjustable boost | replaceable | EN controlled; size for at least 100 mA |
| Charger/path | BQ24074 | replaceable | USB input, LiPo charge, power-path operation |
| Audio output | MAX98357A I2S class-D | replaceable | 5 V supply, mono, short differential speaker route |
| Speaker | 8 ohm, 1 W minimum, 20-28 mm | external | JST-PH 2-pin; enclosure cavity is part of the acoustic design |
| Microphone | IM69D130 digital MEMS | replaceable | bottom-port, isolated opening, I2S/PDM routing per selected mode |
| Persistent storage | on-module 16 MB flash plus microSD | frozen interface | SD is removable content/storage, internal flash holds settings |
| Battery | protected 1-cell LiPo, 1200-2000 mAh | external | JST-PH 2-pin, polarity checked at assembly |
| Controls | Menu, B, encoder A/B + centre A | frozen interface | active-low, hardware pull-ups and RC footprints |
| Power switch | SPDT slide switch | external | hard battery isolation; actuator dimensions remain a release gate |

The display connector pin order, from the Sharp datasheet, is:

| Pin | Signal | PCB treatment |
|---:|---|---|
| 1 | SCLK | 33 ohm source series resistor footprint |
| 2 | SI | 33 ohm source series resistor footprint |
| 3 | SCS | 10 kohm inactive pull-up and source series footprint |
| 4 | EXTCOMIN | timer/GPIO, must continue toggling while image is retained |
| 5 | DISP | pulled low when panel rail is off |
| 6 | VDDA | 5 V; at least 100 nF at connector |
| 7 | VDD | 5 V; at least 1 uF at connector |
| 8 | EXTMODE | strap footprint selects external COM inversion |
| 9 | VSS | digital ground |
| 10 | VSSA | analogue ground, joined to ground plane at connector |

`VDD` and `VDDA` must rise together or digital first, and fall together or digital
first. Firmware must stop display traffic, deassert `DISP`, and stop `EXTCOMIN`
before disabling the 5 V rail.

## 4. GPIO contract

| Function | GPIO | Reason |
|---|---:|---|
| USB D- / D+ | 19 / 20 | native USB; fixed by ESP32-S3 |
| LCD SCLK / SI / SCS | 12 / 11 / 10 | dedicated SPI host group |
| LCD EXTCOMIN / DISP / 5V_EN | 14 / 13 / 15 | timer-capable outputs |
| Encoder A / B / centre A | 4 / 5 / 0 | preserves current T-Embed firmware mapping |
| B / Menu | 47 / 48 | input-only product actions |
| Mic CLK / DATA | 39 / 42 | preserves current firmware mapping |
| Speaker BCLK / LRCLK / DIN | 46 / 40 / 7 | preserves current firmware mapping |
| microSD SCK / MOSI / MISO / CS | 36 / 35 / 37 / 38 | isolated SPI host assignment |
| I2C SDA / SCL | 8 / 18 | charger/fuel-gauge expansion |

GPIO0 is a boot strap. The centre A circuit includes a series resistor and test pad;
the user must not hold A during reset in production firmware. If this proves
unacceptable, D1 moves `A_SW` to a non-strap pin and keeps a hidden boot pad.

## 5. PCB construction and placement

- Main board: 116 x 68 x 1.6 mm, four layers, ENIG, 1 oz outer copper.
- Stack: signal + components / solid GND / power + slow signals / signal + components.
- Minimum design rule: 0.15 mm track/space, 0.30/0.15 mm via, 0.20 mm copper-edge.
- ESP module is on the top edge; its antenna projects into an enclosure RF window.
- No copper, battery, speaker magnet, fast signal, or metal coating enters the module
  antenna keepout on any layer.
- USB-C and ESD parts are adjacent at the top edge. D+/D- are a 90 ohm differential
  pair over uninterrupted ground with minimal stubs.
- Display FPC is above the screen tail location. No vias occur beneath the flex bend.
- Boost converter loop is compact and separated from mic and antenna zones.
- Mic is on the front edge with a gasketed, straight acoustic path. No speaker cavity
  air path reaches the mic port.
- Amplifier is next to the speaker connector. `SPK+/-` remain differential and do
  not connect to ground.
- Main PCB mounting holes are NPTH 2.7 mm for M2.5 screws; use nylon washers near
  the glass display.

The D0 KiCad board intentionally contains placement zones and named interface
footprints rather than an invented production footprint for an encoder we have not
measured. This is a correctness feature, not missing documentation.

## 6. Enclosure and installation

The generated OpenSCAD model is a dimensional envelope and split-shell starting
point. Production shell requirements:

- Outer nominal: 122 x 74 x 19 mm; 2.0 mm walls; 0.35 mm FDM clearance.
- Display opening: 59.2 x 35.7 mm nominal, centred over the 58.8 x 35.28 mm viewing
  area, with a black internal mask hiding the 62.8 x 42.82 mm glass outline.
- Display is held by a four-point compliant frame. No screw clamps glass directly.
- FPC bends only forward, inner radius at least 0.45 mm, with the bend line kept in
  the datasheet-allowed region. Add Kapton plus a smooth printed strain-relief ramp.
- Speaker cavity is sealed to the front grille with a foam gasket. Start at 1.5-3.0
  cubic centimetres and tune grille/open area using a physical acoustic prototype.
- Mic uses its own foam gasket and 0.8-1.2 mm port. Keep at least 25 mm from the
  speaker grille and mechanically isolate it from shell vibration.
- Battery uses a removable foam cradle, never adhesive directly on PCB solder joints.
- USB, switch, microSD, encoder daughterboard, and speaker remain serviceable after
  removing the rear shell.
- Heat-set M2.5 inserts are in the rear shell; bosses do not overlap antenna keepout.

Assembly order:

1. Inspect the display tail under magnification and confirm contact-side orientation.
2. Print the shell fit coupon first: screen aperture, FPC ramp, encoder opening, USB.
3. Assemble and electrically test the bare PCB without display, speaker, or battery.
4. Verify 3V3 and disabled 5V rails; then verify current-limited 5V startup.
5. Fit display with power off, lock ZIF, apply strain relief, then run panel bring-up.
6. Fit mic gasket, speaker gasket/cavity, encoder daughterboard, buttons, and switch.
7. Check battery connector polarity before insertion; perform charge and thermal test.
8. Close shell without forcing glass or FPC; verify both desk-rest angles.

## 7. Release gate

All boxes are mandatory before ordering assembled boards:

- [ ] Physical screen tail pitch, pin count, contact side, glass outline, viewing area,
      tail centreline, and bend direction measured and photographed with calipers.
- [ ] Selected FPC connector mated to a sacrificial sample without reverse contact.
- [ ] Encoder sample measured; daughterboard footprint, shaft height, push travel,
      wheel clearance, and detent torque frozen.
- [ ] Speaker sample impedance/diameter/depth and cavity volume frozen.
- [ ] Battery sample dimensions, lead exit, connector polarity, and protection frozen.
- [ ] Power switch actuator and shell aperture frozen.
- [ ] All LCSC numbers checked in the JLC assembly library on order day.
- [ ] Schematic ERC, PCB DRC, antenna keepout, USB impedance, and power review pass.
- [ ] 3D collision review passes with supplier STEP models.
- [ ] Current, charging, thermal, RF, microphone SNR, speaker SPL, and drop tests pass.
- [ ] A second engineer checks display power sequencing and battery safety.

## 8. Generated deliverables

Run:

```sh
python3 hardware/rev-a/product-d0/generate.py
```

It creates `generated/cadenza-rev-a-product-d0.zip` containing the KiCad import
project, BOM, pick/place intent, DXF outlines, OpenSCAD enclosure, and this release
contract. In JLCEDA Pro choose **File > Import > KiCad**, upload the ZIP, open both
schematic and PCB, and execute the checklist in `generated/JLCEDA_IMPORT.md`.

## 9. Playdate comparison

Playdate validates the product direction rather than providing a board to copy: a
400 x 240 reflective display, very small control vocabulary, purpose-built rotary
input, mono speaker, microphone, Wi-Fi, and a tightly integrated enclosure. Cadenza
keeps those interaction principles but uses an ESP32-S3, a landscape Sharp panel,
an annular encoder with centre A, Menu/B buttons, removable storage, and a larger
serviceable acoustic cavity. The important lesson is to co-design controls, RF,
acoustics, display, and shell; copying the yellow rectangle would miss the point.

