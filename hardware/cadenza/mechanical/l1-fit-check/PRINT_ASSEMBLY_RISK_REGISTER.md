# L1 打印与装配风险清单

> 结果：**PASS_NON_FROZEN_AUDIT**。当前 CAD 已对齐 screen-only PCB，但仍不是可直接打印/打板证明。

## 当前可证明的几何

- Sharp 窗口和玻璃相对旧居中位置在外壳坐标均移动 `+1.5 mm X / +1.5 mm Y`。
- J_DISP1/USB1 来自当前 PCB；courtyard Y 净距 `0.965567 mm`。
- FPC 弯折 keepout 与 FH34/USB1 的代理公共体积为 `{'fpc_bend_keepout_vs_fh34_mm3': 13.693049999999994, 'fpc_bend_keepout_vs_usb1_mm3': 78.99801436799996, 'fpc_bend_keepout_vs_front_outside_candidate_slot_mm3': 130.92349751357452}`，所以折线路径明确保持未完成。
- 前壳已增加与屏幕窗口连通的 FPC 过线槽候选 `[61.68, 9.14, 71.32, 13.36]`；槽口遮蔽与真实排线折弯仍需实体样。
- 74 个 footprint 中 66 个有 F.CrtYd；31 个 XY 投影落入屏幕玻璃。缺少高度和 PCB 安装 Z，不能清除闭壳风险。
- 屏幕窗口对玻璃覆盖 L/R/B/T 为 `[1.4, 1.4, 3.17, 3.17] mm`；软垫 `0.40 mm`、压框 `1.00 mm` 均为代理。

## 风险登记

| ID | 等级 | 信心 | 判断 | 解除条件 |
|---|---|---|---|---|
| MECH-FPC-01 | critical | high | Current FPC bend keepout intersects both the selected FH34 envelope and USB1 envelope; exact folded route and latch access are not proven. | Use the photo-confirmed rear contact face in a 1:1 fold mock-up, then confirm Pin 1 and pass repeated insertion and closed-shell powered test without touching USB1. |
| MECH-PIN1-02 | critical | high | The PCB connector rotation is fixed in the candidate, but the 180-degree fold can reverse the apparent left/right pin order; physical mapping is not closed. | Photograph front/back, continuity-check all ten pins and compare a printed 1:1 pin-number overlay before ordering. |
| MECH-ZSTACK-03 | critical | high | The imported PCB does not provide a trustworthy board-to-front-cover Z datum or complete verified component heights. | Measure PCB seating Z and critical component heights, model all screen-overlap parts, then pass a closed-shell section/collision audit. |
| MECH-USB-04 | high | high | USB1 XY is authoritative but its body height is still a conservative proxy, so FPC-to-USB clearance is not verified. | Identify exact USB MPN or caliper-measure the mounted connector including solder protrusion and repeat the bend audit. |
| MECH-SCREEN-05 | high | high | The 0.40 mm gasket is only geometry; compression, adhesive creep and glass stress are untested. | Select actual gasket/adhesive, print a sacrificial front, and inspect powered pixels after repeated close/open cycles. |
| MECH-RETAINER-06 | high | high | The loose 1.00 mm rear frame has no fastening or preload feature and is not a production retainer. | Choose screws/snaps/adhesive and verify retention, glass load and service sequence in a real print. |
| MECH-SCREW-07 | high | high | Inherited cover Ø3.0 mm bores and PCB Ø2.0 mm holes do not select a screw or printed pilot-hole compensation. | Print a hole/boss coupon, select screw or inserts, and inspect after three assembly cycles. |
| MECH-WALL-08 | medium | medium | Nominal inherited floor/boss sections do not prove local minimum wall or print anisotropy. | Run slicer thin-wall inspection and a material-specific dimensional coupon. |
| MECH-PRINT-09 | medium | medium | Shrinkage, supports, warping and surface finish remain printer/material/orientation dependent. | Print one sacrificial fit shell and record XY hole and Z-stack deviations before release. |

## 边界

- 不因代理 Z 下硬碰撞为零就宣称可生产。
- FPC 出线和金手指面已由用户正反照片确认；仍必须确认折后 Pin 1、USB 高度、PCB 到前壳 Z，并做 1:1 折弯样和通电闭壳测试。
- 打印前还需选材料/打印机，完成孔、薄壁、玻璃受力试片。
