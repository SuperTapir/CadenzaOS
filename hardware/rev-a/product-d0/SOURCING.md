# Rev A D0 sourcing plan

Stock is volatile. These are engineering candidates, not an authorization to order
assembled boards. Recheck manufacturer part number, LCSC number, package, EDA model,
stock, assembly tier, and lifecycle in JLCEDA on the day the BOM is released.

## JLC/LCSC SMT candidates

| Function | Manufacturer part | LCSC | D0 disposition |
|---|---|---:|---|
| MCU/radio | ESP32-S3-WROOM-1-N16R8 | C2913202 | freeze |
| 10-pin FPC | FH34SRJ-10S-0.5SH(50) | C324723 | freeze only after contact-side test |
| charger/power path | BQ24074RGTR | C54313 | preferred |
| 5 V display boost | TPS61023DRLR | C919459 | preferred |
| mono I2S amplifier | MAX98357AETE+T | C910544 | preferred |
| 3.3 V prototype LDO | AP2112K-3.3TRG1 | C51118 or C23380830 | prototype only; 600 mA peak gate |
| digital MEMS mic | IM69D130V01XTSA1 | C42464166 | preferred if stock; PDM, 1.8 V rail required |
| push-push microSD | MICRO SD4.0 10P H1.67 PUSH | C53223917 | candidate; low stock risk |
| USB-C USB2 receptacle | TYPE-C16PIN | C393939 | candidate; shell height must match enclosure |

The microphone candidate is a 1.8 V PDM part, not a 3.3 V I2S microphone. Production
capture must therefore add a quiet 1.8 V rail and verify ESP32-S3 input thresholds,
clock frequency, stereo-select strap, and bottom-port land pattern. If we want to
preserve the current T-Embed electrical implementation instead, use the LilyGO
SPM1423HM4H-B circuit as a reference and source a currently supported equivalent.

## Taobao/sample-first mechanical parts

Search terms and purchase quantities for the first fit kit:

| Item | Suggested search | Buy | Acceptance test |
|---|---|---:|---|
| annular encoder | `T-Embed 编码器 带按键 圆环 24档` / `环形编码器 中心按键` | 3 variants | pinout, shaft height, wheel OD, detent torque, push force/travel, lifecycle |
| speaker | `8欧 1W 23mm 超薄 扬声器 JST PH2.0` | 3 sizes | DC resistance, depth, SPL, bass in 1.5/2/3 cc sealed cavities, buzz/rattle |
| battery | `聚合物锂电池 3.7V 1500mAh 带保护板 PH2.0` | 2 | dimensions, polarity, lead exit, protection cutoff, charge temperature |
| switch | `超薄 拨动开关 自锁 SPDT 侧拨` | 3 variants | actuator reach, force, current rating, panel tolerance |
| speaker gasket | `EVA 泡棉 单面胶 1mm` | sheet | airtight compression and ageing |
| mic gasket | `MEMS 麦克风 防尘网 泡棉` | sheet/kit | port seal, attenuation, wind/pop rejection |

Do not assume JST-PH battery polarity from product photos. Verify every sample with a
multimeter and key the assembly work instruction to the actual wire colours and PCB
polarity mark.

## Prototype quantity

- PCB: 5 bare boards after D1 production capture, assemble 2 first.
- Display: use one existing panel for connector/firmware bring-up; keep one spare.
- Critical ICs: board quantity plus 2 spare units for hand rework.
- FPC connectors: at least 5 because flex mating trials can damage actuators.
- Mechanical candidates: 2-3 variants each before shell geometry is frozen.
- Printed shell: first print individual fit coupons, then one transparent full shell,
  then the material/colour prototype.

## Freeze order

1. Screen/FPC connector and display 5 V sequence.
2. Encoder daughterboard and front wheel geometry.
3. Speaker plus tuned cavity and microphone isolation.
4. Battery, switch, USB-C, and microSD apertures.
5. Power/current/thermal measurements.
6. Exact JLC parts, verified footprints, routing, DRC, Gerber review.
7. Final shell, assembly drawing, fasteners, gasket drawings, and purchasing list.

