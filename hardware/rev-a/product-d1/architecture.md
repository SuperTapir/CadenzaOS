# Cadenza Rev A Product D1 architecture

Status: component freeze candidate, not yet released for fabrication.

## Product-level decisions

D1 is the first production-capture revision. Unlike D0, every electrical block must
resolve to a real manufacturer part, verified symbol, verified land pattern, power
budget, placement region, and test point. The display remains the only immutable
purchased component. Mechanical-feel parts remain sample-first and connect through
stable electrical interfaces.

The Playdate comparison is architectural rather than cosmetic: use a small control
vocabulary, a purpose-built rotary control, a reflective 400 x 240 display, a real
speaker cavity, a gasketed microphone port, edge RF placement, and a serviceable
stack. Cadenza differs through landscape orientation, Menu/B plus annular A control,
ESP32-S3 Wi-Fi/BLE, removable storage, USB audio, and a larger acoustic volume.

## Power tree

```text
USB-C 5 V
  |
  +-- ESD + 1.5 A resettable fuse
  |
  +-- BQ24074 power path/charger ---- protected 1S LiPo
                |
                +-- VSYS (3.5-4.4 V typical while operating)
                       |
                       +-- TPS63070 buck-boost --> +3V3_MAIN / 2 A
                       |       `-- slide switch controls EN; charger remains alive
                       |
                       +-- TPS61023 boost ------> +5V_MEDIA / <=1.5 A
                               `-- firmware EN, default off
```

The 3.3 V rail is no longer based on AP2112. A 600 mA LDO has inadequate margin for
ESP32-S3 Wi-Fi current peaks, storage activity, microphone, and control LEDs, and it
cannot hold 3.3 V near the bottom of a LiPo discharge curve. TPS63070 provides a
regulated rail throughout discharge and load disconnect when off.

### BQ24074 D1 settings

- USB-C is sink-only USB 2.0; CC1/CC2 each use 5.1 kohm Rd.
- No USB-PD negotiation. Input remains 5 V.
- Input current limit defaults to 500 mA for conservative cable/host operation.
- Charge current is 500-600 mA for a 1200-2000 mAh protected cell.
- `RISET` is populated only after the chosen battery's permitted charge current is
  confirmed; valid D1 options are documented in the assembly variant table.
- TS uses the battery's 10 kohm NTC when available; otherwise fit the datasheet-safe
  resistor window only for bench prototypes, never silently defeat temperature
  protection on a production unit.
- `PGOOD`, `CHG`, and `ISET_MON` go to test pads; charge status may also reach GPIO.

### TPS63070 3.3 V settings

- 1.2 uH inductor, saturation current >=4.5 A, low DCR.
- Input and output ceramic capacitance follow the TI reference design, with DC-bias
  derating checked at actual rail voltage.
- Feedback divider is calculated for 3.3 V and stored as explicit BOM values.
- Power switch drives EN through a debounced/pulled network. `PWR_GOOD` is observable.
- Switching loop and return vias stay on the power side of the PCB, away from mic.

### TPS61023 5 V settings

The D1 implementation follows TI's 1-cell Li-ion to 5 V reference: 1 uH inductor,
10 uF input, two 22 uF output capacitors, 732 kohm / 100 kohm feedback divider.
`+5V_MEDIA` feeds the Memory LCD directly and MAX98357A through a ferrite/decoupling
branch. Firmware enables the rail only after +3V3 is valid.

## Compute, radio, and storage

- `ESP32-S3-WROOM-1-N16R8`, LCSC C2913202.
- Module antenna sits at the top enclosure edge with the manufacturer's all-layer
  keepout extended through the battery, shell inserts, speaker magnet, and coatings.
- Native USB D-/D+ use GPIO19/20 and a low-capacitance USB ESD array at the connector.
- USB-C receptacle: HCTL `HC-TYPE-C-16P-01A-G`, C2997435, matching the KiCad official
  footprint `USB_C_Receptacle_HCTL_HC-TYPE-C-16P-01A`.
- Internal settings/database use the module's 16 MB flash through the firmware's
  transactional persistent-storage layer.
- Removable content uses a push-push microSD connector; card detect is routed.
- Optional BQ27441 fuel gauge is fitted in D1 because battery percentage inferred
  only from voltage is poor under Wi-Fi/audio load.

## Display

`LS027B7DH01` electrical and mechanical constraints are immutable:

- 62.8 x 42.82 x 1.64 mm module; 58.8 x 35.28 mm view; 400 x 240.
- 10-pin 0.5 mm FPC.
- Pin order: SCLK, SI, SCS, EXTCOMIN, DISP, VDDA, VDD, EXTMODE, VSS, VSSA.
- VDDA/VDD use +5V_MEDIA; logic remains 3.3 V.
- 33 ohm source footprints on SCLK/SI/SCS; 0 ohm is an assembly option.
- 100 nF on DISP, >=100 nF on VDDA, >=1 uF on VDD at the connector.
- EXTMODE defaults to external inversion; EXTCOMIN is timer-generated.
- Power sequence is encoded both in hardware pull states and firmware contract.
- Connector candidate is FH34SRJ-10S-0.5SH(50), C324723. The final contact-side and
  pin-1 orientation gate still requires mating the owner's physical panel.

## Audio

### Microphone

Preferred performance option is Infineon `IM73D122V01XTMA1`:

- PDM output, 1.62-3.60 V supply, 73 dBA SNR, -26 dBFS sensitivity, 20 Hz LFRO.
- Operated at 3.3 V so ESP32-S3 clock/data require no level translation.
- 1 uF bypass at VDD, dedicated bottom acoustic opening, no trace/via beneath port.
- Clock gets a 22-47 ohm source-series footprint. DATA gets a short direct route.
- SELECT strap has two population options and a test pad.

Supply fallback is active Infineon `IM69D130V01XTSA1` (69 dBA SNR, C42464166).
Because its PG-LLGA-5-1 package is not assumed identical to IM73D122 PG-LLGA-5-3,
the PCB library contains two reviewed footprint variants; only one may be populated.

### Speaker

- MAX98357AETE+T, C910544, I2S mono class-D.
- +5V_MEDIA through a ferrite bead, local 100 nF plus >=10 uF.
- 8 ohm, >=1 W, 20-28 mm external speaker connected by keyed 2-pin connector.
- SPK+ and SPK- are a differential pair and never connect to ground.
- Amplifier is adjacent to speaker connector; high-current loop stays out of mic/RF.
- Enclosure starts with 1.5/2.0/3.0 cc sealed cavity coupons and selects by SPL,
  low-frequency response, distortion, rattle, and microphone feedback testing.

## Controls

- Left `MENU` and `B`: low-profile tact switches, active low, 10 kohm pull-up,
  100 nF optional debounce capacitor, ESD if exposed through metal trim.
- Right `ENCODER_DB`: mechanically independent annular encoder daughterboard.
- Six-pin main-board interface: +3V3, GND, ENC_A, ENC_B, A_SW, LED_CTRL.
- Encoder A/B and centre A have Schmitt-trigger conditioning footprints; DNP by
  default if the sampled encoder waveform is clean.
- GPIO0 is removed from user-facing A in D1. A must not share a boot strap.
- Dedicated recessed BOOT and RESET service pads remain accessible after rear removal.

## Frozen GPIO contract

| Function | GPIO |
|---|---:|
| USB D- / D+ | 19 / 20 |
| LCD SCLK / SI / SCS | 12 / 11 / 10 |
| LCD EXTCOMIN / DISP / 5V_EN | 14 / 13 / 15 |
| Encoder A / B / centre A | 4 / 5 / 6 |
| B / Menu | 47 / 48 |
| PDM MIC CLK / DATA | 39 / 42 |
| Speaker BCLK / LRCLK / DIN / SD | 46 / 40 / 7 / 41 |
| microSD SCK / MOSI / MISO / CS / DET | 36 / 35 / 37 / 38 / 45 |
| I2C SDA / SCL | 8 / 18 |
| Charge status | 17 |

The firmware compatibility layer maps current T-Embed GPIO0 centre-key behavior to
GPIO6 on Rev A hardware. GPIO0 remains boot-only.

## PCB and enclosure stack

- Main PCB 116 x 68 x 1.6 mm, 4-layer ENIG.
- L1 components/signals, L2 continuous GND, L3 power/slow signals, L4 signals.
- Separate encoder daughterboard with measured hole pattern.
- Product envelope 122 x 74 x 19 mm nominal.
- Screen uses compliant four-point support and smooth FPC ramp, never screw pressure.
- Front speaker gasket and mic gasket have no shared air path.
- Rear facets provide approximately 18 and 35 degree desk positions.
- Battery, speaker, encoder, switch, and USB remain replaceable after rear removal.

## D1 release gates

1. Every schematic symbol is checked pin-by-pin against current manufacturer PDF.
2. Every footprint is checked against recommended land pattern and one printed 1:1
   paper/film overlay; fine-pitch parts additionally get a microscope inspection.
3. Power rails pass worst-case current, startup, shutdown, brownout, and thermal tests.
4. Display tail and connector mate without reversal and meet bend-radius constraints.
5. Encoder/speaker/battery/switch samples are measured and frozen.
6. KiCad ERC/DRC are clean with only documented, reviewed exclusions.
7. Gerber and drill are independently viewed; BOM/CPL match board references.
8. KiCad and JLCEDA imports are compared layer/net/footprint/outline/keepout/3D-wise.
9. Two assembled prototypes pass RF, audio, charging, storage, USB, drop, and endurance.

