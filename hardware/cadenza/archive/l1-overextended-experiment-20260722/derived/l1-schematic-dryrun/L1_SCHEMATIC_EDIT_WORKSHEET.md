# L1 原理图 GUI 编辑工作单

状态：**待执行；本文不是已完成记录。**

只允许在本目录的派生 `.kicad_sch` 中执行。开始前运行
`baseline-proof/verify_pre_edit_baseline.py`；保存后必须用 KiCad 10 导出新网表，再运行
`hardware/cadenza/gates/verify_l1_candidate_netlist.py`。

## 1. 修改区域

- 显示区域：旧 `FPC1/U6/C20/C21/Q2/R13/R14` 所在 A3 D 区，约
  `x=28..168 mm, y=191..200 mm`。
- Power/Lock 区域：`U1` 与旧 `SW1` 周围，约 `x=167..240 mm, y=23..33 mm`；
  MCU 感知与强制关机用命名网络连接，不跨页拉长线。
- 按键释放点：删除 `KEY1`（`150.622, 116.078`），只断开 `SW2` pad 5
  （`221.996, 153.924`）。
- 禁止移动 `U1/U4/SW2`、其他受保护器件、图框或 PCB。

## 2. 删除与局部修改

按顺序删除并检查残留线头：

1. `FPC1, U6, C20, C21, Q2, R13, R14`；
2. `SW1`，但保留 `/VOUT` 与 `+5V` 两侧网络，稍后由 `R_PWR6` 桥接；
3. `KEY1`，把 U4 的 GPIO18 网络改名为 `PWR_KEY_SENSE`；
4. `SW2` 只断开 pad 5 与 GPIO6，并在 pad 5 放置明确 no-connect；pad 1/2/3/4/6
   保持原状态；
5. 把 U4 的 GPIO6 网络改名为 `PWR_FORCE_OFF`。

不要执行全局重新注释，也不要 Update PCB from Schematic。

## 3. 标准 KiCad 符号与 footprint 策略

| Designator | 符号 | Value | Footprint 策略 |
|---|---|---|---|
| J_DISP1 | `Connector_Generic:Conn_01x10` | `LS027B7DH01_10P_INTERFACE` | **必须清空** |
| C_DISP1 | `Device:C` | `100nF` | `Capacitor_SMD:C_0805_2012Metric` 候选 |
| C_DISP2 | `Device:C` | `100nF` | `Capacitor_SMD:C_0805_2012Metric` 候选 |
| C_DISP3 | `Device:C` | `1uF` | `Capacitor_SMD:C_0805_2012Metric` 候选 |
| TP_DISP_* / TP_PWR_* | `Connector:TestPoint` | `TESTPOINT` | 本轮可留空，布局阶段选择 |
| D_PWR1 | `Diode:BAT54C` | `BAT54C` | 标准库默认 `Package_TO_SOT_SMD:SOT-23` |
| SW_PWR1 | `Switch:SW_Push` | `NO_MOMENTARY_SIDE_PUSH` | **留空，实物机械门禁后选择** |
| R_PWR1 | `Device:R` | `1k` | `Resistor_SMD:R_0805_2012Metric` 候选 |
| R_PWR2 | `Device:R` | `10k` | 同上 |
| Q_PWR1 | `Transistor_FET:AO3400A` | `AO3400A` | 标准库默认 `Package_TO_SOT_SMD:SOT-23` |
| R_PWR3 | `Device:R` | `100R` | `Resistor_SMD:R_0805_2012Metric` 候选 |
| R_PWR4 | `Device:R` | `100k` | 同上 |
| R_PWR5 | `Device:R` | `0R` | 默认 DNP；封装候选 0805 |
| R_PWR6 | `Device:R` | `0R_HIGH_CURRENT_TBD` | **留空，额定电流确认后选择** |

本机 KiCad 10.0.4 标准库已确认包含 `BAT54C`、`AO3400A`、`Conn_01x10`、
`TestPoint` 和 `SW_Push`。BAT54C 标准符号 pin name 为空，但图形/编号为 1、2 两阳极、
3 共阴极；AO3400A 继承 `Q_NMOS_GSD`，1=G、2=S、3=D。最终仍以对应厂家数据手册
和候选网表门禁为依据。

## 4. Sharp 连接表

| J_DISP1 pin | 信号 | 网络 |
|---:|---|---|
| 1 | SCLK | GPIO48 |
| 2 | SI | GPIO12 |
| 3 | SCS | GPIO14 |
| 4 | EXTCOMIN | GPIO47 |
| 5 | DISP | GPIO39 |
| 6 | VDDA | +5V |
| 7 | VDD | +5V |
| 8 | EXTMODE | +5V |
| 9 | VSS | GND |
| 10 | VSSA | GND |

`C_DISP1` 接 GPIO39-GND；`C_DISP2`、`C_DISP3` 均接 +5V-GND。七个显示
测试点分别接 +5V、GND、GPIO48、GPIO12、GPIO14、GPIO47、GPIO39。

## 5. Power/Lock 连接表

- `U1.5 KEY`、`D_PWR1.1`、`Q_PWR1.3`、`R_PWR5.1`、`TP_PWR_KEY.1`
  → `PWR_IP5306_KEY`。
- `D_PWR1.2`、`R_PWR1.1` → `PWR_SENSE_DIODE`。
- `D_PWR1.3`、`SW_PWR1.1`、`R_PWR5.2`、`TP_PWR_BUTTON.1`
  → `PWR_BUTTON_NODE`。
- `SW_PWR1.2` → GND。
- `R_PWR1.2`、`R_PWR2.2`、U4 GPIO18、`TP_PWR_SENSE.1`
  → `PWR_KEY_SENSE`；`R_PWR2.1` → 3V3M。
- U4 GPIO6、`R_PWR3.1`、`TP_PWR_FORCE.1` → `PWR_FORCE_OFF`。
- `R_PWR3.2`、`R_PWR4.1`、`Q_PWR1.1` → `PWR_GATE`；
  `R_PWR4.2`、`Q_PWR1.2` → GND。
- `R_PWR6.1`、`TP_PWR_VOUT.1` → `/VOUT`；
  `R_PWR6.2`、`TP_PWR_5V.1` → `+5V`。

`R_PWR5` 必须设置 DNP。不要把 `PWR_IP5306_KEY` 或 `PWR_BUTTON_NODE` 直接接到
ESP32 GPIO；BAT54C 就是隔离边界。

## 6. 保存后的强制门禁

1. 关闭 Eeschema，让文件写入完成；
2. 确认 `working-base`、`golden-import` 未变化；
3. KiCad 10 导出 XML 网表；
4. 候选网表合同 17 项全部通过；
5. 导出 ERC JSON，与 691 项编辑前基线按类型、严重度和位置做差分；
6. 导出 PDF/SVG并逐页检查残留线头、错误 no-connect、重叠文字和网络名；
7. 再次用 KiCad 10 打开/关闭，重复导出，端点集合必须稳定；
8. 仍不选择 FPC footprint，不同步 PCB，不勾选依赖实物证据的 OpenSpec 任务。
