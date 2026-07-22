# L2 可打印控制件候选（未冻结）

本目录把原先的“实心包络代理”推进为**可切片、可做手感/配合试验的参数化候选件**，但它们不是最终生产件。它只读取现有 `l2-input-candidates` 的 A/B/C 坐标和对应前壳，不修改正式 KiCad、OpenSpec 或 L1 外壳。

## 已生成的控制件

- 独立 B 键帽：圆形键帽、穿壳导向柱、假设的轻触开关执行头插孔、无字体依赖的凹刻 B 标识。
- 独立 Menu 键帽：较小键帽、导向柱、执行头插孔和三横线标识。
- A/编码器旋钮：整个旋钮随 EC12 轴向按下，顶面以几何凹槽标出 `A`；这就是“中心 A”的视觉与按压接口，不另加一个会与 EC12 中心按压冲突的按钮。
- 三类可替换轴接口：
  - `round`：假设 Ø6 mm 圆轴的摩擦孔；
  - `d-shaft`：假设 Ø6 mm、0.8 mm 平面深度的 D 孔；
  - `knurled-adapter`：旋钮采用带防转键的大插孔，另打印一个十二边形内孔的牺牲适配芯。实测滚花后只需改适配芯，不必重画旋钮外形。

三套 A/B/C 布局均生成三种轴接口的组合 STEP/STL，共九套装配候选。A/B/C 坐标仍来自上游机械探索，“A balanced 首选”不等于冻结。

## 打印方向与当前间隙假设

- 单件 STL 已摆成“带标识的外表面朝打印床，导向柱/轴套向上”。这样无需给轴套内部加支撑，但 0.4 mm 喷嘴可能模糊第一层凹字；它是试样方向，不是最终工艺。
- 当前按 FDM、0.4 mm 喷嘴、0.2 mm 层高、每侧约 0.10 mm XY 试配间隙建立。
- B/Menu 导向柱相对现有壳孔直径保留约 0.35 mm/侧。
- 旋钮轴套相对现有 Ø7.8 壳孔保留约 0.35 mm/侧。
- 未按下时键帽距壳 0.65 mm、旋钮距壳 0.8 mm；验证脚本又将 B/Menu 向内移动 0.35 mm、A 旋钮向内移动 0.5 mm，检查按到底的硬碰撞。

这些数字是试印参数，不是从已购实物得到的尺寸。

## 仍待实物验证

在用户用卡尺确认以下数据前，不得把这里的孔径或轴长复制到最终 PCB/外壳：EC12 的圆轴/D 轴/滚花类型、轴径、D 平面、滚花齿数、轴露出长度、螺纹套尺寸、中心按压行程；B/Menu 开关执行头直径/高度/行程；PCB 到前壳的真实高度；所选材料与打印机的孔缩补偿。

因此本目录没有勾选 OpenSpec 5.1、5.2、5.6 或 5.7，也不声称 STL 已能直接装到用户手上的器件。

## 文件

- `parameters.json`：唯一人工参数源和全部“待验证”假设。
- `generated/common/`：B、Menu 键帽以及可替换滚花适配芯。
- `generated/{a-balanced,b-compact,c-thumb-arc}/`：每套布局的三类旋钮与九套组合装配。
- `generated/printable-controls-manifest.json`：输入哈希、文件清单、每套包络/按压和碰撞结果。
- `MESH_AND_ASSEMBLY_REPORT.json`：STEP BRep 与 STL 网格复验结果。
- `VALIDATION_REPORT.md`：九套装配的碰撞、屏幕净距和整体包络摘要。
- `render/`：由 STL 独立生成、无外部 CAD 渲染依赖的 SVG 目视复核图；不是制造输入。

## 重复生成与验证

```sh
PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools \
  /Users/tapir/.local/share/uv/python/cpython-3.12.11-macos-aarch64-none/bin/python3.12 \
  hardware/cadenza/mechanical/l2-printable-controls/generate_printable_controls.py

PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools \
  /Users/tapir/.local/share/uv/python/cpython-3.12.11-macos-aarch64-none/bin/python3.12 \
  hardware/cadenza/mechanical/l2-printable-controls/verify_printable_controls.py

python3 hardware/cadenza/mechanical/l2-printable-controls/render_previews.py
```

验证覆盖：所有 STEP 可重新读入且 BRep 有效、所有 ASCII STL 无退化三角形/开边/非流形边、九套候选在未按下和代理按压行程末端均不与前壳硬碰撞、安装孔 keepout 无碰撞、屏幕面板二维净距不低于 3 mm、选择状态仍为未冻结。
