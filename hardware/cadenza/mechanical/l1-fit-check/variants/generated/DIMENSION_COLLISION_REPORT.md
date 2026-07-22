# L1 FPC 候选尺寸与碰撞报告

> 状态：未冻结。数据手册已解决屏幕局部方向，但外壳中 0°/180° 旋转、转接板映射和连接器位置仍未选择。

| 候选 | 出线 | 触点面 | FPC XY 包络 (mm) | 压框避让 XY (mm) | 六组硬碰撞最大值 (mm³) |
|---|---:|---|---|---|---:|
| `fpc-plus-y-top-contact` | +Y | +Z / toward display front and front cover | X 60.68–69.32, Y 51.41–57.47 | X 60.18–69.82, Y 50.80–58.61 | 0.000000 |
| `fpc-plus-y-bottom-contact` | +Y | -Z / toward enclosure interior and PCB | X 60.68–69.32, Y 51.41–57.47 | X 60.18–69.82, Y 50.80–58.61 | 0.000000 |
| `fpc-minus-y-top-contact` | -Y | +Z / toward display front and front cover | X 60.68–69.32, Y 2.53–8.59 | X 60.18–69.82, Y 1.39–9.20 | 0.000000 |
| `fpc-minus-y-bottom-contact` | -Y | -Z / toward enclosure interior and PCB | X 60.68–69.32, Y 2.53–8.59 | X 60.18–69.82, Y 1.39–9.20 | 0.000000 |

共同名义尺寸：屏幕玻璃 62.80 × 42.82 × 1.64 mm；FPC 平直包络宽 8.64 mm、伸出玻璃 6.06 mm、厚 0.30 mm。弯折提示区从玻璃边 0.80 mm 延伸到 6.00 mm，最小内弯半径按 R0.45 mm 记录。

碰撞检查覆盖前壳/玻璃、前壳/FPC、前壳/压框、软垫/玻璃、玻璃/压框和 FPC/压框。四个候选的这些实体硬碰撞均为 0。触点面标记与 FPC 本体有意贴合，不作为独立结构件参与碰撞判定。

待验证：真实出线方向、裸露触点面、Pin 1、转接板 1:1 连续性、连接器型号与高度、卡扣空间、自然折叠路线和合壳余量。

## 数据手册约束后的两套外壳旋转候选

Sharp 原厂 LCY-1210401A 规格书 PDF 第 26 页（规格页 23）明确：显示面朝上时 FPC 从 6 点钟方向伸出；平直尾部触点在背面，正面视图 Pin 1 在右侧；只能向背面弯折。平直接法推荐 bottom-contact，按 Figure 8-2 向背面折叠后推荐 top-contact。

| 候选 | 6点钟映射 | Pin 1 全局侧 | 边缘走廊 | 平直尾后剩余 | FH12A 包络 | 实体硬碰撞 | 路径状态 |
|---|---:|---:|---:|---:|---:|---:|---|
| `datasheet-plus-y-folded-top-contact` | +Y | +X | 8.59 mm | 2.53 mm | 9.1×6.2×2.0 mm | 0.000000 mm³ | PENDING_EXACT_FOLD_ROUTE_AND_LATCH |
| `datasheet-minus-y-folded-top-contact` | -Y | -X | 8.59 mm | 2.53 mm | 9.1×6.2×2.0 mm | 0.000000 mm³ | PENDING_EXACT_FOLD_ROUTE_AND_LATCH |

两套候选的连接器与前壳、软垫、玻璃、对应开槽压框、FPC 和 PCB 代理实体硬碰撞均为 0，且连接器 XY 包络位于 130×60 mm PCB 内。可是 8.59 mm 边缘走廊减去 6.06 mm 平直尾只剩 2.53 mm，不能把 6.2 mm 深连接器简单接在尾端之后；连接器包络还与保守弯折 keepout 相交。因此精确折线路径、锁扣开合和真实 PCB Z 高度仍必须待验证，不能因实体 0 碰撞冻结。
