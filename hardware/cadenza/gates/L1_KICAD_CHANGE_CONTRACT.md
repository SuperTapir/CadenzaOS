# L1 KiCad 受控修改契约

> 当前适用范围：screen-only L1 的 PCB 几何/封装边界检查。旧的
> `l1-schematic-delta-contract.json` 包含已取消的 Power/Lock，已经封存，不再是真值源。
> 当前原理图与 PCB 实施边界以
> [L1_SCREEN_ONLY_DELTA_CONTRACT.md](L1_SCREEN_ONLY_DELTA_CONTRACT.md) 为准。

目的：后续替换显示子系统时，允许预期变化，但自动发现对参考板其他功能的误伤。它补充 ERC/DRC，不替代数据手册、实物方向、EMC、DFM 或首板验证。

## 允许修改的参考器件

- `FPC1`：旧 12P TFT 座，删除或替换为 Sharp 10P 座。
- `U6`：旧 `3V3LCD` 稳压器，删除。
- `C20`、`C21`：保留封装、坐标、旋转、料号和接地端，只把电源端从 `3V3LCD` 改为 `+5V`，分别复用为 Sharp VDD/VDDA 去耦。
- `Q2`、`R13`、`R14`：旧背光驱动，删除；GPIO39 改为 Sharp DISP。

上述列表之外的 70 个参考封装在 L1 显示适配中锁定：不得消失、移动、旋转、换封装或改变焊盘网络。C20/C21 也不得移动、旋转或换料。板框包络和四个安装孔锁定。

## 新显示器件允许连接的网络

`GND`、`+5V`、`GPIO48`、`GPIO12`、`GPIO14`、`GPIO47`、`GPIO39`。

旧 `3V3LCD`、背光匿名网络和显示用 GPIO3 可以消失，但不得把新器件接到它们。新器件若连接其他网络，机器门禁判定失败，并要求人工说明或修正。

## 使用方法

KiCad Python 路径来自当前安装的 KiCad 10.0.4：

```sh
KICAD_PY=/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/3.9/bin/python3.9

$KICAD_PY hardware/cadenza/gates/l1_change_boundary.py verify \
  path/to/L1-candidate.kicad_pcb \
  --baseline hardware/cadenza/gates/l1-change-boundary.json \
  --output path/to/l1-boundary-verification.json
```

通过条件：七项检查全部为 `true`。首次建立的基线还要用同一块参考 PCB 自检，结果保存在 `l1-change-boundary-self-check.json`。

## 该门禁没有证明什么

- 没有证明 FPC 顶/底接点、Pin 1 或自然弯折方向正确。
- 没有证明 Sharp 初始化、电源时序或 EXTCOMIN 工作。
- 没有检查新增走线是否越过天线净空、切断地回流或违反嘉立创线宽/间距。
- 没有检查 BOM、CPL、极性、旋转和 3D 模型。

这些项目必须在后续实物验证、KiCad DRC/人工复核、EMC 和制造门禁中分别完成。
