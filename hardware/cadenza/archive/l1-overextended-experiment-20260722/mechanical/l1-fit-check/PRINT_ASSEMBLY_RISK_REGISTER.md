# L1 打印与装配风险清单

> 结果：**PASS_NON_FROZEN_AUDIT**。这只表示候选几何和风险记录可重复复查，不表示外壳可生产。FPC 方向、接点面和 Pin 1 仍未冻结。

## 可直接从 STEP/候选 JSON 证明的尺寸

- 底盒四个孔为名义 `Ø3.0 mm`，同心螺柱外径 `Ø7.0 mm`，径向壁厚 `2.0 mm`，圆柱面跨度约 `11.0 mm`。
- 底盒大面水平面显示名义底厚 `1.60 mm`。这些是局部名义截面，**不是全局最小壁厚证明**。
- Sharp 窗口对玻璃边缘的覆盖 L/R/B/T 为 `[1.4, 1.4, 3.17, 3.17] mm`；后压框捕捉量为 `[1.6, 1.6, 3.41, 3.41] mm`。
- 后压框厚 `1.00 mm`，软垫仅是 `0.40 mm` 代理。两者都没有真实材料/压缩证据。
- L2 三套控件代理的最小屏幕/外缘/孔位 keepout 间隙为 `6.60 / 9.00 / 8.69 mm`；B/Menu 最小边缘间隙 `2.75 mm`。这只适用于代理体。

## 风险登记

| ID | 等级 | 信心 | 判断 | 解除条件 |
|---|---|---|---|---|
| MECH-FPC-01 | critical | high | Sharp p.23 resolves the screen-local 6-o'clock exit, rear flat-contact face, Pin 1 side and allowed rear fold; enclosure +Y/-Y rotation, adapter mapping, exact connector latch and folded route remain unknown. | Photograph the real display/adapter, continuity-check all ten adapter pins, choose enclosure rotation, then validate connector latch and folded route in a closed enclosure. |
| MECH-SCREEN-02 | high | high | The 0.40 mm gasket is a geometric proxy; compression, adhesive creep and glass stress are untested. | Choose actual gasket material from supplier data and pass repeated close/open plus powered display inspection without pressure artefacts or glass damage. |
| MECH-RETAINER-03 | high | high | The 1.00 mm retainer has no fastener or snap geometry and is thinner than the inherited 1.60 mm nominal floor section. | Select material/process, add fastening, print and pass flex/retention testing. |
| MECH-SCREW-04 | high | high | Inherited exact Ø3.0 mm bores may print undersize; screw type and pilot-hole rule are not selected. | Print a hole/boss coupon, select screw/inserts and record final compensated bore; inspect bosses after three assembly cycles. |
| MECH-WALL-05 | medium | medium | Nominal inherited sections are 1.60 mm floor and 2.00 mm boss radial wall, but local global minimum and print anisotropy are not proven. | Run slicer thin-wall preview for the chosen nozzle/resin and physically inspect all walls/bosses; thicken only derived enclosure if needed. |
| MECH-INPUT-06 | high | high | Button bodies, keycaps, EC12 body/shaft and press travel remain proxies. | Measure the purchased EC12 and switches with calipers; validate axial press at full travel and rotation in a 1:1 print. |
| MECH-POWER-07 | high | high | Power/Lock placements clear current proxies but switch body, cap travel and accidental-press force are unverified. | Select the real switch and pass reach, guard, full-travel and closed-shell tests. |
| MECH-ZSTACK-08 | critical | high | PCB components, solder joints, battery, speaker, connector latch and wiring are not included in a closed-shell Z-stack. | Build a complete component-envelope assembly and pass closed-shell collision/clearance audit. |
| MECH-PRINT-09 | medium | medium | Shrinkage, hole compensation, support scars and warping depend on the chosen printer/material/orientation. | Print a dimensional coupon and one sacrificial fit shell; record measured compensation before final STL export. |

## 容差策略（非冻结）

- 不对整件等比缩放来“补偿收缩”；用真实打印机/材料做孔、轴、薄壁和 Z 高度试片，分项记录补偿。
- 玻璃不做刚性压入配合；间隙和软垫压缩必须由材料数据与实物合壳确定。
- 名义 Ø3.0 mm 孔不直接当作可装 M3 的证据；先选螺丝/热熔螺母，再按厂商建议与试片定孔。
- 切片时必须检查薄壁是否生成足够周长线；`1.00 mm` 后压框当前明确为待重设计/实测项。

机器可读结果：`L1_PRINT_ASSEMBLY_RISKS.json`。
