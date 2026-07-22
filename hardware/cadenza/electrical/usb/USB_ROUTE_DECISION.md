# Cadenza L2 USB 路线候选决策

状态：**候选，未冻结，未修改 KiCad，未实测**  
审计日期：2026-07-22  
推荐：**L1 保留参考板 CH340C；L2 改为 ESP32-S3 原生 USB，并保留手动 UART/下载恢复焊盘。**

## 1. 一句话解释

参考板的 USB-C 数据线目前只连到 CH340C，ESP32-S3 的原生 USB 脚 GPIO19/20
被原摇杆占用。L2 本来就会删除这个摇杆，因此 GPIO19/20 可以释放出来接原生
USB。切换后必须删除 CH340C 的数据路径，不能让 CH340C 和 ESP32-S3 两个 USB
收发器同时挂在同一对 D+/D− 上。

这个决定不表示原参考板有严重问题。CH340C 路线已随参考样机运行，是 L1 风险最低
的下载方式；原生 USB 是面向最终 Cadenza 功能和器件精简的 L2 改动，需要单独打样
验证。

## 2. 参考工程的真实连接

证据来自派生工作基线：

- 原理图：`hardware/cadenza/working-base/Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.kicad_sch`
- PCB：`hardware/cadenza/working-base/Cadenza-reference-ESP32-S3-game-console-original-v2-20260722.kicad_pcb`
- 结构化原理图结果：`hardware/cadenza/analysis/l1-baseline/schematic.json`

| 功能 | 当前连接 | 结论 | 置信度 |
|---|---|---|---|
| USB-C 数据 | USB1 A6/B6 → `D+` → U3 CH340C pin 5；USB1 A7/B7 → `D-` → U3 pin 6 | 当前是 USB 转 UART，不是 ESP32-S3 原生 USB | 高：原理图与 PCB pad net 一致 |
| UART | U3 TXD pin 2 → `U0RXD` → U4 pin 36；U3 RXD pin 3 → `U0TXD` → U4 pin 37 | 参考板通过 UART0 下载和看日志 | 高 |
| 自动下载 | U3 `DTR#`/`RTS#` → Q1 UMH3N → `GPIO0`/`CHIP_PU` | Q1 控制进入下载模式和复位 | 高 |
| 手动下载 | BOOT1 把 GPIO0 拉低；RST1 把 CHIP_PU/EN 拉低 | 删除 CH340C 后仍可手动进入 ROM 下载模式 | 高 |
| 原生 USB 脚 | U4 pin 13/GPIO19、pin 14/GPIO20 → SW2 pin 2/3 | 目前被原摇杆占用，未接 USB | 高 |
| USB-C CC | CC1、CC2 各自通过 R1/R2 `5.1 kΩ` 接 GND | 已按 USB-C 受电设备（Sink/UFP）基本接法连接 | 高 |
| USB ESD | D+/D−/VBUS 未找到专用 USB ESD 器件 | 生产候选应补 | 中高：原理图无保护器件；自动检查也报 VBUS ESD 缺失 |
| 外壳焊脚 | USB1 pad 13/14 无网络 | 金属外壳目前没有明确泄放路径 | 高 |
| 测试点 | 工程中没有正式测试点 | UART 恢复完全依赖板载 CH340C 和按键 | 高 |

PCB 只读抽取还显示：当前 CH340C 的 D+、D− 各约 33.5 mm、无过孔；GPIO19、
GPIO20 到 SW2 的旧输入走线分别约 127.9 mm 和 113.8 mm。切换原生 USB 时不能
沿用这些长输入线，必须删除后重新布成一对短的差分线。

## 3. 为什么 L2 推荐原生 USB

1. ESP32-S3 官方定义 GPIO19=`USB D−`、GPIO20=`USB D+`，支持 USB Serial/JTAG、
   下载和 USB Device 功能。
2. L2 会移除 SW2，GPIO19/20 的现有冲突会自然消失；当前 GPIO 候选表也已专门为
   原生 USB保留这两个脚。
3. 删除 CH340C 和 Q1 后减少一个 USB PHY、一个桥接 IC 和一组自动下载网络；未来若
   需要 CDC、HID、MIDI 或音频类设备，不必再改 USB 数据硬件。
4. 官方同时提醒：应用若关闭或改用 USB PHY，自动 USB 下载可能失效。因此不能把
   原生 USB 当作唯一救援入口，必须保留 GPIO0、EN 和 UART0 的物理控制点。

如果 L2 的目标临时缩减为“只要求稳定烧录与串口日志，不需要任何 USB Device
功能”，继续保留 CH340C 仍然是更低风险的回退选项；但它不是当前最终推荐。

## 4. L2 应继承什么

以下是电气继承候选，不等于采购料号已经冻结：

- USB1 的板边位置、外壳开口基准和 USB-C 2.0 receptacle 连接概念。
- USB1 的 A6+B6 并成 D+、A7+B7 并成 D−；同名触点应在连接器附近汇合。
- R1、R2：CC1、CC2 各 `5.1 kΩ` 到 GND；建议最终采用 1% 阻值。
- VBUS → IP5306 输入及现有输入电容的总体供电架构，但输入电流合规性仍需验证。
- U4 ESP32-S3-WROOM-1-N16R8 最小系统、GPIO0/EN 上拉及 RC。
- BOOT1、RST1 手动按键。
- `U0TXD`、`U0RXD`、`GPIO0`、`CHIP_PU` 网络，只改变它们的外部终点。
- USB1 的精确料号仍是待验证：现有资料在 C49261586/C49261588 之间冲突，不能仅凭
  footprint 冻结采购。

## 5. L2 应删除什么

| 对象 | 动作 | 原因 |
|---|---|---|
| U3 `CH340C` | 删除 | 避免第二个 USB PHY；原生 USB 不再需要桥接 |
| Q1 `UMH3N` | 删除 | 它只服务 CH340C 的 DTR/RTS 自动下载 |
| `DTR`、`RTS` 网络 | 删除 | 原生 USB 没有这两条调制解调器控制线 |
| USB1→U3 的旧 `D+`/`D-` 铜线 | 删除并重布 | 新终点变为 GPIO20/GPIO19 |
| SW2 | 按 L2 输入改造删除 | 原摇杆不再存在 |
| GPIO19/20→SW2 的全部旧铜线 | 删除 | 不能把按钮或长悬空支线挂在 USB 差分线上 |
| U3 专用局部丝印/测试标记 | 删除或更新 | 防止生产与维修误判 |

只有在确认某颗 3V3 去耦电容确实专用于 U3 后才能随 U3 删除；不能按“离 U3 较近”
猜测删除共用电容。

## 6. L2 应新增什么

### 6.1 原生 USB 数据通道

```text
USB1 A7+B7 (D−)
  → D_ESD1 通道 1
  → R_USB_DM 22 Ω（初值）
  → U4 GPIO19 / USB_D−

USB1 A6+B6 (D+)
  → D_ESD1 通道 2
  → R_USB_DP 22 Ω（初值）
  → U4 GPIO20 / USB_D+
```

- `D_ESD1` 候选：ST `USBLC6-2SC6`，SOT-23-6L，LCSC `C7519`。它是面向
  USB 2.0 的双数据线低电容 ESD 器件，并提供 VBUS 钳位连接。**采购状态和 JLCPCB
  Basic/Extended 分类必须在下单前重查。**
- `R_USB_DM`、`R_USB_DP`：各 `22 Ω`、1%、0402/0603，紧贴 U4 放置。官方允许
  22/33 Ω 起始值；首板可预留改为 33 Ω 的调试空间。
- `C_USB_DM`、`C_USB_DP`：各预留一个对地小电容焊盘，首板 `DNP`，紧贴 U4；
  未经信号完整性/EMC 结果不得随意贴电容。
- 网络明确命名为 `USB_D-` 和 `USB_D+`，不要继续使用容易与 CH340 历史路径混淆的
  裸 `D-`/`D+`。

### 6.2 ESD 与连接器外壳

- D_ESD1 应在 USB1 旁边，连接器到保护器件的未保护铜线尽可能短；它的 GND 使用
  短而宽的路径并就近打地过孔。
- D_ESD1 的 VBUS pin 接 `USB_VBUS_RAW`，GND pin 接 GND；不要把保护器件放在
  22 Ω 电阻的 MCU 一侧。
- USB1 金属壳 pad 13/14 建立 `USB_SHIELD` 网络。塑料外壳、无独立机壳地的首板候选：
  用 `R_SHIELD=0 Ω` 接本地 GND，并在连接器旁加地过孔；同时预留 `C_SHIELD` 焊盘
  为 DNP，便于 EMC 样机调整。最终直连还是 RC 连接必须经 ESD/EMC 实测冻结。

### 6.3 VBUS 检测（电池供电设备）

Cadenza 可在没有 USB 线时由电池继续运行，因此按 USB Device 的定义属于自供电设备。
ESP-IDF 5.5 官方要求监测 VBUS：

```text
USB_VBUS_RAW → R_VBUS_TOP_EQ 75 kΩ, 1% → USB_VBUS_SENSE → GPIO7
USB_VBUS_SENSE → R_VBUS_BOTTOM 100 kΩ, 1% → GND
```

`R_VBUS_TOP_EQ` 有两个互斥装配候选：单颗 75 kΩ `C17819`（Extended），或两颗
150 kΩ `C17470`（Basic）并联。只要原理图明确互斥，二者的名义电气值相同；当前优先
双 150 kΩ，以一个额外 0805 位置换取不新增 Extended 料种。PCB 空间不足时可回到
单颗 75 kΩ。最终 BOM/CPL 不得把两种方案同时贴装。

- GPIO7 在当前 L2 候选表中未分配；它现阶段只连将被删除的 SW2 pin 1，因此与屏幕、
  B/Menu、编码器、Power/Lock、音频、UART0 和原生 USB均不冲突。
- 该比例在 VBUS=4.4 V 时名义输出约 2.51 V，在 VBUS=5.5 V 时约 3.14 V。
- 检测节点首板不加电容。固件使用 `self_powered=true` 和
  `vbus_monitor_io=GPIO7`。
- 官方还要求拔线后检测脚 3 ms 内变低。现有 VBUS 上有 10 µF，实际放电速度未知，
  所以这组阻值只是候选；必须用示波器测 `USB_VBUS_SENSE`。若不满足，改用满足阈值
  与释放时间的比较器/隔离检测方案，不能靠软件延时掩盖。

### 6.4 恢复 UART / 下载焊盘

新增一个不装连接器的 `RECOVERY1` 六焊盘 pogo/测试接口，候选针序：

| Pad | 网络 | 外部治具方向/用途 |
|---:|---|---|
| 1 | GND | 共地 |
| 2 | `U0TXD` | ESP32 输出；接外部 USB-UART RX |
| 3 | `U0RXD` | ESP32 输入；接外部 USB-UART TX，**仅 3.3 V 逻辑** |
| 4 | `GPIO0` | 治具拉低后复位，进入下载模式 |
| 5 | `CHIP_PU` | 治具短暂拉低复位 |
| 6 | `3V3M_SENSE` | 只作电压检测；默认禁止从治具反向给整板供电 |

同时保留 BOOT1 和 RST1，所以没有治具时也能按“按住 BOOT → 点按 RST → 松开
BOOT”进入 ROM 下载。RECOVERY1 应放在拆开后可接触的位置，并在底部丝印标出
`G TX RX IO0 EN 3V3`。UART0 TX 可按 Espressif 建议预留约 `499 Ω` 串联电阻，
是否贴装由首板波形与日志可靠性决定。

不要在 USB_D+/D− 上增加长支线式测试点。若生产测试必须探测数据线，只允许无支线、
小尺寸的同线探针 pad，并在最终布线审计中复核阻抗。

## 7. PCB 布线门禁

- D+/D− 必须作为一对布线，目标差分阻抗 `90 Ω ±10%`；线宽/间距要按嘉立创实际
  板厚、铜厚和阻焊参数计算，不能直接抄参考板 0.254 mm 线宽。
- A/B 两组 USB2 触点在 USB1 附近合并；D_ESD1 紧邻连接器，串联电阻紧邻 U4。
- 差分线等长、平行、尽量短、尽量不换层；如果必须换层，两线同时换层并配置成对地
  回流过孔。
- 差分线下方保持连续 GND，不跨地平面裂缝，不穿过天线 keepout，不靠近 Class-D
  扬声器输出、开关电源电感或高速 SPI 时钟。
- 删除 GPIO19/20 到 SW2 的旧长线后再布 USB，禁止留下 stub。
- 两层板可以做，但底层在 USB 路径下应优先保持完整地平面。

## 8. 电源与 USB-C 的独立待验证项

R1/R2 的 5.1 kΩ 只声明“这是一个受电设备”，不会自动告诉 IP5306 当前 USB-C 电源
允许取 0.5 A、1.5 A 还是 3 A。参考板样机能充电，是有价值的系统证据，但并不能单独
证明所有电脑和充电器上的输入电流都合规。

生产冻结前必须二选一并验证：

1. 把 IP5306 的输入/充电电流限制在所支持的保守默认电流；或
2. 增加能识别 USB-C Source Current/BC1.2 的检测与限流方案。

这项问题与选择 CH340C 还是原生 USB 无关，但两种路线都共享同一个 USB-C/VBUS，
因此不能遗漏。

## 9. 明确禁止的连接

- 禁止 USB1 D+/D− 同时直连 CH340C 和 GPIO19/20。
- 禁止用 GPIO 矩阵把原生 USB 改到别的脚；内部 PHY 固定使用 GPIO19/20。
- 禁止 GPIO19/20 同时保留按钮、编码器或 LED 负载。
- 禁止将 USB VBUS 5 V 直接接 ESP32 GPIO。
- 禁止把外部 5 V UART 接入 U0RXD/U0TXD。
- 禁止在没有测量拔线放电时间前宣称 VBUS 检测满足规范。

## 10. 实施前后验证清单

实施前：

- [ ] 用 USB1 实物/尺寸图冻结精确连接器 MPN、外壳焊脚与 Pin 1。
- [ ] 决定 IP5306 的 USB 输入限流策略。
- [ ] 在 DevKit 或独立样机验证 GPIO19/20 原生 USB 下载、CDC/目标 USB class。
- [ ] 用故意关闭 USB PHY 的固件验证 BOOT/RST + RECOVERY1 UART 确实能救板。

KiCad 修改后：

- [ ] 网络审计证明 USB1 D+/D− 只到 D_ESD1 → 串阻 → GPIO20/19。
- [ ] U3、Q1、DTR、RTS 和 SW2 的 GPIO19/20 铜线全部消失。
- [ ] BOOT1、RST1、UART0 和 RECOVERY1 连通。
- [ ] ERC/DRC 后人工复核 USB-C pinout、ESD pinout 和 ESP32 模组 pad 号。
- [ ] 根据嘉立创 stack-up 计算并复核 90 Ω 差分线。
- [ ] 回导/生产预览确认 USB1、D_ESD1、串阻和 RECOVERY1 方向。

首板：

- [ ] 正反插都能供电、枚举、刷写和看日志。
- [ ] 电池供电时插拔 USB 能正确连接/断开，测得 VBUS sense 拔线下降时间。
- [ ] 固件异常时能通过手动下载与恢复 UART 救回。
- [ ] 测 USB 充电输入电流、连接器温升和 IP5306 行为。
- [ ] 做连接器接触放电/系统级 ESD 与预兼容 EMC 检查。

## 11. 依据与审计边界

主要资料：

- [Espressif ESP32-S3 硬件设计指南：USB 原理图与 GPIO19/20](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html)
- [Espressif ESP32-S3 PCB 指南：90 Ω 差分、连续参考地与回流](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/pcb-layout-design.html)
- [Espressif 下载指南：USB 失效条件及保留 UART 的建议](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/download-guidelines.html)
- [ESP-IDF 5.5 USB Device：自供电设备 VBUS 检测](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32s3/api-reference/peripherals/usb_device.html)
- [USB-IF USB Type-C 2.0：Sink 的 CC1/CC2 Rd 模型](https://www.usb.org/sites/default/files/USB%20Type-C%20Spec%20R2.0%20-%20August%202019.pdf)
- [ST AN5225：USB2 Type-C 受电设备两颗 5.1 kΩ Rd 与可翻转数据脚合并](https://www.st.com/resource/en/application_note/an5225-introduction-to-usb-typec-power-delivery-for-stm32-mcus-and-mpus-stmicroelectronics.pdf)
- [ST USBLC6-2SC6 数据手册](https://www.st.com/resource/en/datasheet/usblc6-2.pdf)

审计限制：本次是 USB 子系统定向审计，不是整板设计审查；未运行 SPICE（USB 数字 PHY
不适合用当前模拟子电路流程证明）和 EMC 全板分析（L2 原理图/PCB 尚未实现）。通用 PCB
分析器因导入板内一个空 zone polygon 中止，关键 pad net 和走线则由 KiCad 解析器直接
复核。所有“新增”内容均为实施合同，不是已经画入 PCB 的事实。
