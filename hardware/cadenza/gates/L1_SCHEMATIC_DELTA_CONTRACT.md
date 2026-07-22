# L1 KiCad 原理图修改合同

> **已封存 / 不再适用于当前 L1。** 本文包含已取消的 Power/Lock、SW1、KEY1、SW2 和测试点改造，只作为 2026-07-22 以前实验的历史记录。当前 L1 的唯一合同是 [L1_SCREEN_ONLY_DELTA_CONTRACT.md](L1_SCREEN_ONLY_DELTA_CONTRACT.md)。不得继续用本文或对应旧 JSON 生成生产候选。

状态：**候选合同，尚未实施，尚未冻结**。本合同只规定“以后如何修改”；它没有修改 `working-base`、没有选择 FPC 封装，也不证明 PCB 可下单。

机器可读真值源：[l1-schematic-delta-contract.json](l1-schematic-delta-contract.json)  
合同验证器：[verify_l1_schematic_delta_contract.py](verify_l1_schematic_delta_contract.py)

## 1. 合同覆盖范围

本合同把冻结参考网表中的 77 个器件和 102 个网络完整分区，同时加入两个 L1 必需变化：

1. 以 Sharp `LS027B7DH01` 10-pin 接口替换旧 TFT 显示子系统；
2. 增加独立 Power/Lock 瞬时键、MCU 短按感知和硬件长按关机路径。

L1 继续使用参考板的 CH340C USB/下载路线。ESP32-S3 原生 USB、GPIO19/20 释放和完整 SW2 删除属于 L2，不提前混入本合同。

## 2. 参考器件动作

### 删除 9 个

| 器件 | 原值 | 原因 |
|---|---|---|
| FPC1 | 12P、0.5 mm TFT FPC | 换成 Sharp 10P 接口 |
| U6 | ME6210A33M3G | Sharp 的 VDD/VDDA 使用 5 V，不使用旧 3V3LCD |
| C20/C21 | 10 µF / 100 nF | 属于旧 3V3LCD 支路 |
| Q2/R13/R14 | SS8050 / 2.2 kΩ / 10 kΩ | Sharp 无背光 |
| SW1 | SK12D07VG4-BF | 旧件串联切断 `/VOUT → +5V`，不符合单一瞬时 Power/Lock 交互 |
| KEY1 | HX TS665WS | 明确释放 GPIO18 给 `PWR_KEY_SENSE` |

### 局部修改 1 个

`SW2` 不在 L1 整体删除。仅把 pad 5 与 GPIO6 断开，以便 GPIO6 用作 `PWR_FORCE_OFF`：

- 必须保留 footprint、坐标和 GPIO7/19/20/GND 端点；
- pad 6 继续是原 no-connect；
- pad 5 必须显式 no-connect 或确认没有残留铜线；
- 这会有意牺牲旧摇杆的一个方向，L1 不承诺旧摇杆功能；
- 完整删除 SW2 并把 GPIO19/20 改为原生 USB，仍留到 L2。

这种“部分保留”有实现风险：原理图 no-connect、PCB 残留 stub 和 footprint 焊盘网络必须在真正编辑时复核。

### 原样保留 67 个

完整清单在 JSON 的 `reference_components.retain`。其中禁止顺手重构的核心边界为：

- IP5306 充电/升压核心，唯一例外是启用 `U1.5 KEY` 和替换 SW1 串联断点；
- TLV62568 3.3 V 电源；
- ESP32-S3-WROOM-1-N16R8 最小系统、BOOT、RST；
- USB1、CH340C、UMH3N 自动下载、D+/D−、DTR/RTS、UART0；
- MAX98357A、扬声器接口；
- microSD；
- 参考麦克风 U7（仅继承，不新增“料号已正确”结论）；
- RGB LED；
- 四个安装孔及机械基准。

器件动作三组互斥，且 `9 删除 + 1 修改 + 67 保留 = 77`。

## 3. Sharp 10-pin 唯一引脚合同

| Sharp Pin | 名称 | 目标网络 | 电压域 |
|---:|---|---|---|
| 1 | SCLK | GPIO48 | ESP32 3.3 V 输出 |
| 2 | SI | GPIO12 | ESP32 3.3 V 输出 |
| 3 | SCS | GPIO14 | ESP32 3.3 V 输出 |
| 4 | EXTCOMIN | GPIO47 | ESP32 3.3 V 输出 |
| 5 | DISP | GPIO39 | ESP32 3.3 V 输出 |
| 6 | VDDA | +5V | 显示模拟电源 |
| 7 | VDD | +5V | 显示数字电源 |
| 8 | EXTMODE | +5V | 外部 COM 模式固定高 |
| 9 | VSS | GND | 数字地 |
| 10 | VSSA | GND | 模拟地 |

明确禁止把 Pin 6/7 接 `3V3M`，也禁止把 `+5V` 接到任意 ESP32 GPIO。3.3 V GPIO 驱动 5 V 供电的 Sharp 数字输入是数据手册允许的既定跨域方式，仍需面包板和首板验证波形。

FPC 上接/下接、触点面、Pin 1 物理方向和具体 footprint 均为**待验证**。合同只固定电气 pin number → net，不固定铜焊盘朝向。

## 4. 显示新增器件

- `J_DISP1`：Sharp 10P 电气接口，footprint 必须为空，直到实物门禁通过；
- `C_DISP1 = 0.1 µF`：DISP → GND；
- `C_DISP2 ≥ 0.1 µF`：VDDA/+5V → GND，靠近 Pin 6；
- `C_DISP3 ≥ 1 µF`：VDD/+5V → GND，靠近 Pin 7；
- 七个测试点：+5V、GND、SCLK、SI、SCS、EXTCOMIN、DISP。

## 5. Power/Lock 候选合同

候选 GPIO：

| 功能 | GPIO | 参考冲突的明确处理 |
|---|---:|---|
| PWR_KEY_SENSE | 18 | 删除 KEY1，不把 GPIO18 描述成原本空闲 |
| PWR_FORCE_OFF | 6 | 只断开 SW2 pad 5；GPIO7/19/20 仍按 L1 参考路线保留 |

候选拓扑：

```text
U1.KEY ---- D_PWR1 BAT54C A1
                         K(common) ---- PWR_BUTTON_NODE ---- SW_PWR1 ---- GND
3V3M -- R_PWR2 10k -- PWR_KEY_SENSE -- R_PWR1 1k -- BAT54C A2

GPIO6/PWR_FORCE_OFF -- R_PWR3 100R -- PWR_GATE -- Q_PWR1 AO3400A gate
                                          |
                                      R_PWR4 100k
                                          |
                                         GND
Q_PWR1 source = GND; drain = U1.KEY

/VOUT -- R_PWR6（高电流 0 Ω，规格待冻结）-- +5V
R_PWR5（0 Ω，默认 DNP）跨接 U1.KEY 与 PWR_BUTTON_NODE，作为 BAT54C 压降救援旁路
```

`BAT54C` 候选为 C916424，pin 1/2=A1/A2，pin 3=共阴极；`AO3400A` 候选为 C20917，pin 1/2/3=G/S/D。它们仍须通过 KEY 低电平、反向供电、USB 插入、三档电池电压下短按和长按关机实测，不能由本合同直接冻结。

Power/Lock 增加六个测试点：U1.KEY、公共按键节点、MCU 感知、强制关机、`/VOUT`、`+5V`。

## 6. 网络动作

JSON 精确记录了参考 102 个网络：

- 删除 10 个：旧 `3V3LCD`、旧 GPIO3 显示网、两条背光网、三个 FPC1 no-connect 和三个 SW1 no-connect；
- 修改 12 个：`+5V/GND/3V3M//VOUT`、五个显示 GPIO、GPIO18、GPIO6 和 U1.KEY no-connect；
- 原样保留 80 个，包括 CH340C USB/自动下载、GPIO7/19/20、音频、存储和其余 no-connect；
- 新增 Power/Lock 内部网：`PWR_BUTTON_NODE`、`PWR_SENSE_DIODE`、`PWR_GATE`，并把 U1.KEY 命名为 `PWR_IP5306_KEY`。

`10 删除 + 12 修改 + 80 保留 = 102`。同一参考网络不能同时属于删除、修改和保留。

## 7. 禁止触碰的边界

- 不编辑 `golden-import` 或 `working-base`；正式修改必须建立新的派生工程。
- 原理图合同阶段不移动 PCB 器件、不改板框、安装孔或既有布局。
- 不改 IP5306/TLV62568 功率核心，除合同列出的 U1.KEY 和 SW1 桥接边界。
- L1 不改 CH340C 的 USB、DTR/RTS、UART0 和自动下载。
- 不允许 CH340C PHY 与 ESP32-S3 原生 USB PHY 并联。
- 不改 MAX98357A 或 microSD 网络。
- 不根据候选库猜 FPC 触点面和 Pin 1。
- 不因合同自检通过而勾选 OpenSpec 任务或宣称可生产。

## 8. 自检

运行：

```sh
python3 hardware/cadenza/gates/verify_l1_schematic_delta_contract.py \
  --output hardware/cadenza/gates/l1-schematic-delta-contract-self-check.json
```

验证器检查：冻结文件哈希；77 个器件和 102 个网络是否被完整、互斥分区；新增 designator/目标网络唯一性；GPIO 重复和保留/启动/PSRAM 冲突；Sharp pin map 与 5 V/3.3 V 域；三颗电容；测试点；Power/Lock 隔离；L1 CH340C 边界；SW2 局部改动是否仍保留 GPIO7/19/20。

PASS 只说明合同本身与冻结参考网表一致。真正画完原理图后还必须另做候选原理图网表差分、ERC、数据手册复审和实物验证。
