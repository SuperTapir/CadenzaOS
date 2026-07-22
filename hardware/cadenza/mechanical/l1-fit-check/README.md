# L1 screen-only 外壳 fit-check

这是从参考 `顶盖V7.step` 派生、并与当前 `Cadenza-L1-Screen-Only.kicad_pcb` 对齐的非冻结机械样件。它不是最终工业设计，也不是“可以直接打印/打板”的证明。

## 当前已对齐

- PCB 外框和四个安装中心不变。
- Sharp 玻璃/窗口相对旧居中位置在外壳坐标移动 `+1.5 mm X / +1.5 mm Y`；这对应 KiCad 的 `X +1.5 mm / Y -1.5 mm`。
- Sharp 窗口仍为 `60.0 × 36.48 mm`，对 `58.8 × 35.28 mm` 可视区每边留 `0.60 mm`。
- J_DISP1 直接读取当前 PCB：KiCad `(154.625, 128.500)`、旋转 `0°`。
- USB1 保持 KiCad `(153.125, 136.121)`；J_DISP1 与 USB1 courtyard 的 Y 向净距约 `0.966 mm`。
- 前壳增加了一个与窗口相连的 FPC 过线槽候选；否则正面安装的屏幕排线无法进入壳内。槽口外观、遮蔽和真实折弯仍待实体样确认。
- CAD 输出包含前壳、软垫、Sharp 当前放置、后框代理、PCB 平面、FH34/USB 包络、全板 F.CrtYd 投影和组合 STEP/STL。

## 当前明确不能推出的结论

- 代理 Z 下“硬碰撞 0”不代表闭壳可行。导入 PCB 没有可信的 PCB 到前壳安装 Z，也没有全部元件的已验证高度。
- 74 个 footprint 中 66 个有 F.CrtYd；其中 31 个 courtyard 的 XY 投影落入 Sharp 玻璃范围。必须补齐高度和真实板壳距离。
- FPC 折弯 keepout 同时与 FH34 和 USB1 包络相交。当前约 `0.966 mm` 的 courtyard 平面净距不能替代真实折弯、锁扣操作和装配公差验证。
- 用户正反面照片已确认金手指在屏幕背面、FPC 从显示面朝上时的下边缘自然伸出；180° 折叠后的 Pin 1 映射、真实折线和锁扣操作仍是实物门槛。
- `0.40 mm` 软垫与 `1.00 mm` 后框均为代理；后框尚无固定与预压结构。

## 可重复生成

```sh
sh hardware/cadenza/mechanical/l1-fit-check/build_and_verify_l1_fitcheck.sh
```

该脚本依次执行下面五步；需要单独排查时也可分别运行：

```sh
/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
  hardware/cadenza/mechanical/l1-fit-check/extract_candidate_pcb_mechanical.py

PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools-py313 \
  /opt/homebrew/bin/python3.13 \
  hardware/cadenza/mechanical/l1-fit-check/generate_l1_front_fitcheck.py

PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools-py313 \
  /opt/homebrew/bin/python3.13 \
  hardware/cadenza/mechanical/l1-fit-check/audit_l1_fitcheck.py

PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools-py313 \
  /opt/homebrew/bin/python3.13 \
  hardware/cadenza/mechanical/l1-fit-check/generate_l1_risk_register.py
```

主要机器证据：

- `generated/candidate-pcb-mechanical.json`
- `generated/l1-front-fitcheck-parameters.json`
- `L1_FITCHECK_AUDIT.json`
- `L1_PRINT_ASSEMBLY_RISKS.json`

`variants/` 是早期方向探索资料，不再是当前 PCB 放置的权威输入；当前机械结论只取上述 screen-only PCB 提取结果。真实 FPC/Pin 1 未确认前，任何方向图都不能冻结生产装配。

断电实物核对请按 100% / Actual Size 打印 [Cadenza L1 FPC 实物方向检查纸](/Users/tapir/Development/CadenzaOS/output/pdf/cadenza-l1-fpc-physical-fit-sheet-a4.pdf)，并先确认其中 50.00 mm 校准线的实测长度。
