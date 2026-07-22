# L1 FPC 方向候选

这里保存 **未冻结** 的机械候选。Sharp 原厂数据手册已经回答屏幕自身的方向问题：

1. 显示面朝上时，FPC 固定从屏幕 6 点钟边伸出；
2. 平直 FPC 的裸露触点朝屏幕背面，显示面朝上视图中 Pin 1 在右侧；
3. FPC 只能向屏幕背面折，不能向正面偏光片折；平直接法对应 bottom-contact，折向背面后对应 top-contact。

仍未决定的是：把屏幕在外壳里旋转为“6 点钟映射 `+Y`”还是旋转 180° 映射 `-Y`。这会同时改变 Pin 1 的全局 X 侧。因此脚本额外生成两个明确标记的 datasheet-backed 外壳旋转候选，各自带 FH12A 最大包络和候选 PCB 平面代理。

脚本会生成 `+Y/-Y × top/bottom-contact` 四套组合 STEP/STL，并为两个方向各生成一个对应开槽的后压框和一个保守弯折禁入包络。`generated/fpc-direction-variants.json` 保存全部尺寸和逐项硬碰撞体积。

旧的四套 `+Y/-Y × top/bottom-contact` 仍保留用于观察，但不再表示四个同等可能的屏幕事实。“接点”只是 0.02 mm 薄标记。新增两套 datasheet-backed 候选使用 9.1×6.2×2.0 mm FH12A 保守连接器包络；这不是精确锁扣模型。

两套连接器包络都位于 PCB XY 内，和前壳、玻璃、软垫、开槽压框、FPC、PCB 代理的实体硬碰撞为 0。但边缘走廊只有 8.59 mm，减去 6.06 mm 平直 FPC 只剩 2.53 mm，不能简单把 6.2 mm 深的连接器顺接在尾端；连接器还与保守弯折 keepout 相交。所以真实折叠路径、PCB Z 高度和锁扣空间仍未冻结。

原厂证据逐项说明见 [DATASHEET_ORIENTATION_EVIDENCE.md](./DATASHEET_ORIENTATION_EVIDENCE.md)。

重复生成：

```sh
/opt/homebrew/bin/python3.13 \
  hardware/cadenza/mechanical/l1-fit-check/variants/generate_fpc_direction_variants.py
```

脚本使用隔离的 OpenCascade 运行库 `/Users/tapir/.cache/cadenza-cad-tools-py313`，不依赖 KiCad 内嵌 Python。

重复验证（只读检查现有输出，并刷新验证报告）：

```sh
/opt/homebrew/bin/python3.13 \
  hardware/cadenza/mechanical/l1-fit-check/variants/verify_fpc_direction_variants.py
```

运行成功的门槛：旧四个观察候选与两个 datasheet-backed 旋转候选均生成；所有 STEP 通过 OpenCascade 有效性检查；实体硬碰撞为 `0`；两套旋转均保持未选择；弯折路线必须保持 `PENDING_EXACT_FOLD_ROUTE_AND_LATCH`。尺寸与碰撞摘要在 `generated/DIMENSION_COLLISION_REPORT.md`，机器可读结果在 `generated/fpc-direction-variants.json` 和 `generated/verification-report.json`。

下一步请按 [实物确认清单](./PHYSICAL_CONFIRMATION_CHECKLIST.md) 做最少实物核对。完成前不应在两个外壳旋转候选中选择最终方向。
