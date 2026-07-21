# Cadenza D1 Rev A v50 完整设计审查

日期：2026-07-21  
对象：`cadenza-d1-rev-a-production.kicad_pcb`、`cadenza-d1.kicad_sch`、v50 制造与外壳发布包

## 结论

**结论：可作为 Rev A 工程首板候选，不是量产冻结版。**

已确认没有 CAD 文件损坏、KiCad ERC/DRC 违规、未连接焊盘、Gerber 缺层、BOM/CPL 数量漂移或 SPICE 失败。底面 74 个贴片器件原先没有全局 fiducial，这一真实装配问题已修复：`FID4-FID6` 位于 B.Cu，原先落在板外的 `FID3` 已移回板内。

下单前仍必须人工确认：

- LS027B7DH01 实物 FPC 的接触面方向、插入方向和 pin 1 方位。
- 嘉立创专业版导入后的板框、层叠、过孔、差分对、封装面与旋转；当前没有本地导出的 `.epro/.epro2` 回读证据。
- 电池极性、扬声器极性、编码器子板线序和连接器实物方向。
- 首板热、EMC、麦克风灵敏度、扬声器响度与外壳声腔测试。

## 本轮修正

- 将 `FID3` 从板外 `(112,118)` 移至板内 `(58,100)`。
- 新增三枚底面全局 fiducial：
  - `FID4 (68,68)`
  - `FID5 (120,62)`
  - `FID6 (58,100)`
- 三枚底面 fiducial 均为 1 mm 裸铜、2 mm 阻焊开窗，排除出 BOM。
- 重填全部覆铜；KiCad 原生 DRC 为 0，未连接焊盘为 0。
- 重新导出 11 个 Protel 扩展 Gerber 层、PTH/NPTH 钻孔和 KiCad/JLCEDA 导入 ZIP。
- Gerber 自动警告由 3 条降为 2 条；底面无 fiducial 的装配错误已消失。

## 设计基线

| 项目 | 当前值 |
|---|---|
| 板框 | 116 mm x 64 mm，四角 R6 |
| PCB | 4 层，1.6 mm |
| 层叠 | F.Cu / In1.Cu(GND) / In2.Cu(POWER) / B.Cu |
| 原理图器件 | 82 |
| IC | 8 |
| 网络 | 79 |
| PCB 主贴装面 | B.Cu，74 个贴片封装 |
| 嘉立创核心 SMT | BOM/CPL 均为 60 个 placement |
| 测试点 | 9 个，自动统计覆盖 9/74 个信号网络 |
| 安装孔 | 4 x 2.2 mm NPTH |
| PTH / NPTH | 164 / 9 |

## 电源树

```text
USB-C VBUS
  |
  +-- U2 BQ24074 power-path charger
       |-- VBAT <-> 1S LiPo
       +-- VSYS
            |
            +-- U3 TPS63070 buck-boost -> +3V3_MAIN
            |     +-- ESP32-S3
            |     +-- microphone / microSD / fuel gauge / controls
            |
            +-- U4 TPS61023 boost -> +5V_MEDIA
                  +-- FB1 -> +5V_AUDIO -> MAX98357A
                  +-- FB2 -> +5V_LCD   -> LS027B7DH01
```

反馈网络 SPICE 结果：

- `R20/R21`：VFB 预期 0.60096 V，仿真 0.601 V。
- `R10/R11`：分压比 0.241935，仿真与解析值误差约 0%。
- `R80/C80`：低通截止频率预期 15.92 Hz，含 PCB 寄生后为 15.8777 Hz，误差 0.27%。

## 实际执行的分析

| 分析 | 结果 |
|---|---|
| KiCad ERC | 0 |
| KiCad DRC | 0，0 unconnected |
| `analyze_schematic.py` | 53 条原始发现；4 error 均经手册判为误报 |
| `analyze_pcb.py --full` | 31 条原始发现；2 error 为有意的边缘连接器/开关 courtyard |
| `cross_analysis.py` | 5 条；4 条表层覆铜/回流路径发现经层叠复核降级 |
| `extract_parasitics.py` | 65 个网络，走线总 R 6.669 ohm，过孔总 L 103 nH |
| `simulate_subcircuits.py` | ngspice 46，100 次容差设置，15 pass / 0 warn / 0 fail / 0 skip |
| `analyze_emc.py` | EU / CISPR Class B、SPICE enhanced；75 条原始发现 |
| `analyze_thermal.py` | SKIPPED；结构化热参数缓存不足 |
| `analyze_gerbers.py --full` | 0 error / 2 warning |
| `lifecycle_audit.py` | 30 MPN；LCSC-only 无生命周期字段，状态均 unknown |
| 外壳碰撞检查 | PCB 不穿透前壳；后壳仅与支撑柱平面接触 |

原始机器输出位于本目录的 `schematic.json`、`pcb.json`、`parasitics.json`、`spice.json`、`cross-analysis.json`、`emc.json`、`thermal.json`、`gerber.json` 和 `lifecycle.json`。

## 数据手册核验

| Ref | MPN | 本地证据 | 状态 |
|---|---|---|---|
| U1 | ESP32-S3-WROOM-1-N16R8 | `datasheets/ESP32-S3-WROOM-1-N16R8.pdf` | 模组供电、USB/GPIO 与天线 keepout 已复核 |
| U2 | BQ24074RGTR | `datasheets/BQ24074RGTR.pdf` | ILIM/ISET/TS/热调节与 RGT-16 + EP 引脚已复核 |
| U3 | TPS63070RNMR | `datasheets/TPS63070RNMR.pdf` | 1 uH 电感、2.4 MHz、反馈与 RthetaJA 63 C/W 已复核 |
| U4 | TPS61023DRLR | `datasheets/TPS61023DRLR.pdf` | 1 uH 电感、1 MHz、反馈与热限制已复核 |
| U5 | MAX98357AETE+T | `datasheets/MAX98357AETE_T.pdf` | 2.5-5.5 V 供电；数字输入 VIH(min)=1.3 V |
| U6 | IM73D122V01 | `datasheets/IM73D122V01XTMA1.pdf` | PDM、VDD/LR/GND 与声孔方向已复核 |
| U7 | BQ27441DRZR-G1A | `datasheets/BQ27441DRZR-G1A.pdf` | 电池检测、I2C 与 DRZ-12 + EP 引脚已复核 |
| U8 | USBLC6-2SC6 | `datasheets/USBLC6-2SC6.pdf` | USB D+/D-、VBUS、GND 保护拓扑已复核 |
| J2/LCD | LS027B7DH01 | `display-freeze-evidence.md` | 10-pin 电气 pinout 已冻结；物理接触方向仍需实物门禁 |

2 引脚无极性 R/C/L/FB 的“pinout”不具备审核意义，按 Skipped 处理；连接器和有极性器件必须在装配前按实物方向表复核。

### MAX98357A 电平误报裁决

原理图分析器把 U5 的 5 V 功放供电域错误地套到 I2S 输入，因而报告 `AMP_EN/I2S_BCLK/I2S_DIN/I2S_LRCLK` 需要 5 V 到 3.3 V 电平转换。MAX98357A 数据手册第 6 页给出的数字音频输入 `VIH(min)=1.3 V`，ESP32 的 3.3 V 输出满足要求。因此这 4 条为 Datasheet-verified false positive，**不得据此增加电平转换器**。

## SPICE 与电源完整性

15 个被识别子电路全部通过，包括：

- 4 个分压/反馈检测项。
- 7 组去耦网络。
- 2 组上电浪涌模型。
- 1 个 RC 低通。
- 1 个稳压反馈网络。

该仿真证明被建模的无源网络和理想化寄生参数自洽，但不等于完整开关电源环路、负载瞬态和扬声器电声模型已通过。首板仍需示波器测量 `+3V3_MAIN`、`+5V_MEDIA`、`+5V_AUDIO` 和 `+5V_LCD` 的启动、纹波与峰值跌落。

## EMC / SI 审查

### Reviewer overrides

- 自动工具报告 GND 有 2 个岛并称 USB/I2C 跨平面缝隙。它检测的是 F.Cu 表层覆铜；真正的相邻参考层 In1.Cu 为单一区域，填充率 97.6%，RF 天线 keepout 是唯一大净空。因此 USB 回流中断结论不成立。
- 25 条 `GP-001` 大多来自同一层选择问题，EMC risk score 22/100 不应作为板级通过/失败分数。
- J1 courtyard 外伸 1.18 mm、SW1 外伸 1.0 mm 是边缘 USB-C 和侧拨执行器的有意机械要求，已由外壳开孔承接。
- `R21:2`、`U8:2` 的 via-in-pad “untented”告警没有读取板级 front/back tenting=yes，属于误报。

### 保留的真实风险

- `PDM_CLK` 路径约 109.4 mm，属于较长数字时钟；首板应检查串扰和麦克风底噪。
- `LCD_SCLK` 与 `I2S_LRCLK` 存在换层且附近 1 mm 内没有专用 GND 回流孔。Rev A 可先验证，若 EMC/音频噪声不佳，Rev B 在每个高速信号换层孔旁补 GND via。
- U3 TPS63070 的 2.4 MHz 谐波会落入 30-88 MHz 辐射频段；必须保持输入/电感/开关节点回路紧凑，并在首板做近场探头扫描。
- U4 去耦距离被评为“moderately far”；应实测 5 V 媒体轨启动和扬声器大音量瞬态。
- U8 附近仅一个主要接地过孔。USB ESD 回流应尽可能短直；首板通过后可在 Rev B 增加邻近地孔。

## 热设计

自动热分析没有可量化的结构化功耗缓存，因此结果是 **SKIPPED，不是 PASS**。

手册边界：

- TPS63070：工作结温 -40 至 125 C，RthetaJA 约 63 C/W。
- TPS61023：工作结温 -40 至 125 C；不同封装/板条件下 RthetaJA 约 91.4-142.7 C/W。
- BQ24074：125 C 附近进入热调节；高充电电流下 PCB 散热直接决定实际充电电流。

U2 EP 当前有 4 个热过孔。自动规则建议 5 个，但没有给出与该小型 EP 几何相符的手册依据；保留 4 个，要求首板在 5 V 输入、低电量电池、屏幕和 Wi-Fi 同时工作时测 U2/U3/U4/U5 温升。若壳内 40 C 环境下任一器件结温估算接近 100 C，应降低充电/音频功率或扩大铜面积。

## Gerber / DFM / 装配

完整层：

- `F.Cu / GND / POWER / B.Cu`
- `F.Mask / B.Mask`
- `F.Paste / B.Paste`
- `F.Silkscreen / B.Silkscreen`
- `Edge.Cuts`
- PTH 与 NPTH drill

Gerber 两条剩余 warning 的裁决：

- “铜层宽度相差 11.1 mm”：层内容边界不同，不是坐标原点错位；RF 天线 keepout 和内层退边会自然造成铜图形 bounding box 不同。
- “F.Paste flashes 仅为 F.Cu 的 28%”：顶面包含连接器通孔、安装孔、显示占位和无锡膏铜项目；不能用铜 flash 数直接推导漏钢网。底面核心 CPL/BOM 仍须在嘉立创预览逐项检查。

制造关注项：

- `R73` 距板边约 0.57 mm。拼板连接筋和 V-cut 不得布置在该区域；优先使用铣边并让工厂工程确认。
- `/PACKP` 最小线宽 0.15 mm，符合常规 4 层工艺但余量较小。
- 测试点覆盖率 12% 适合功能样机，不适合量产 ICT。当前应至少保留电源、USB、启动和关键总线的示波/飞线可达性。
- 三枚底面 fiducial 已加入，顶面也有三枚板内 fiducial。

## 外壳与安装

- 外形：116 x 64 mm PCB，前壳 7 mm、后壳 14.5 mm，总厚约 21.5 mm。
- 4 个 M2 安装孔与壳体支撑柱对齐。
- 前壳、后壳和旋钮 STL 均为 manifold。
- PCB 精确对齐网格与前壳无碰撞；后壳仅在支撑柱顶面产生零厚度接触，这是预期承托。
- J4/J5 保留已核验的 2 A JST-PH 立式连接器；因此后壳深度从 12 mm 增至 14.5 mm。侧入替代件不是 drop-in，不应在 Rev A 临时替换。
- 麦克风声孔、扬声器孔阵、USB-C、microSD、侧拨开关、B/Menu、屏幕和右侧滚轮+A 组合均需在打印首壳上做实装验收。

## 首板验收顺序

1. 裸板目检与短路测试：VBUS、VBAT、VSYS、3V3、5V_AUDIO、5V_LCD 对 GND。
2. 限流电源上电，不装电池；确认 BQ24074 power-path 与 3.3 V/5 V 时序。
3. 接 1S LiPo，核对 JST 极性、充电电流、U2 温升和关机漏电。
4. 刷写最小固件，验证 USB CDC/UAC2、Wi-Fi、BLE、按键、Menu、滚轮+A、microSD。
5. 接 LCD 前再次核对 FPC 接触面和 pin 1；先限流，再验证 VCOM/EXTCOMIN 与画面方向。
6. 测麦克风噪声底、灵敏度、PDM_CLK 串扰；测扬声器扫频、峰值响度和 5 V 轨跌落。
7. 装壳检查按键行程、滚轮同心度、声孔密封、连接器插拔和桌面多角度支撑。
8. 做近场 EMC 扫描与 2.4 GHz 天线 RSSI 对比，再决定是否补回流地孔和滤波。

## 未执行 / 审查边界

- 未完成结构化 `datasheets/extracted/` 全器件缓存，因此热分析无法量化。
- 未做开关电源厂商模型的环路稳定性与最坏条件瞬态仿真；当前 SPICE 主要覆盖无源网络和简化模型。
- 未做实验室 CISPR/FCC、ESD、EFT 或射频认证测试。
- 未做真实电池、LCD、麦克风、扬声器和 3D 打印壳的实物联合测试。
- 未获得嘉立创导入工程的本地 `.epro/.epro2` 回导文件，无法做 KiCad 与嘉立创工程的机器差分。
- LCSC-only 生命周期接口不提供 active/NRND/EOL 字段，30 个 MPN 的 lifecycle 状态仍为 unknown；库存冻结不等于生命周期核验。

## 发布文件

- PCB：`../kicad/production-v42-priority/cadenza-d1-rev-a-production.kicad_pcb`
- 原理图：`../kicad/cadenza-d1.kicad_sch`
- 嘉立创/KiCad 导入包：`../kicad/production-v50-release-20260721/cadenza-d1-v50-kicad-import.zip`
- Gerber：`../kicad/production-v50-release-20260721/cadenza-d1-v50-gerbers.zip`
- 钻孔：`../kicad/production-v50-release-20260721/cadenza-d1-v50-drill.zip`
- JLC BOM/CPL：`../kicad/production-v50-release-20260721/assembly/`
- 外壳 STL/STEP/渲染：`../kicad/production-v50-release-20260721/mechanical/`

