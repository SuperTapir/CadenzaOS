# Cadenza D1 v40 设计审查

状态：**禁止投板；电气重构进行中**。日期：2026-07-21。

## 1. 结论

v40 的 KiCad DRC 为零、嘉立创可导入，但这只证明文件和几何一致，不能证明实物电路正确。制造前审查发现多个确定性阻断项，因此 v38/v40 只能作为导入与布线实验，不能采购或生产。

## 2. 已运行分析

- `analyze_schematic.py`：69 项，其中 6 error、8 warning。
- `analyze_pcb.py --full`：43 项，其中 4 error、21 warning。
- `cross_analysis.py --stage pre_fab`：3 项，其中 2 warning。
- `analyze_thermal.py`，35 C 环境：82/100；旧 R73 模型估算过热。
- `analyze_gerbers.py --full`：文件齐全；铜层有效图形宽度差告警属于天线净空/局部铜形状启发式结果。
- `analyze_emc.py --market eu --compact`：风险模型 0/100；经人工分流后，去耦、参考面缺口和高速换层回流是有效整改方向，谐波“必然落入频段”不是合规失败判定。
- KiCad ERC/DRC：此前均为零，但已证明不能发现错误的自定义符号物理引脚。

未运行：SPICE，当前环境没有 ngspice/LTspice/Xyce；生命周期审查尚未完成；实验室 EMC、热像和真机电源测试必须等首板。

## 3. 制造阻断项

| 严重度 | 问题 | 证据 | 处理 |
|---|---|---|---|
| CRITICAL | U7 BQ27441 自定义符号引脚错位 | TI SLUSBH1C 第 4 页；原理图 raw-file | 重建 1-12 引脚映射 |
| CRITICAL | R73 未串入电池主电流路径，无法计量电流 | TI SLUSBH1C 第 4、15、18 页；原理图 raw-file | 改为 PACKP→10mohm→VBAT 高边采样与 Kelvin 线 |
| CRITICAL | R73 使用 0603，而 TI 推荐 1 W、1%、50 ppm | TI SLUSBH1C 第 16 页；PCB raw-file | 改 2512 1 W 精密分流器 |
| CRITICAL | RUN_EN 开路时浮空，电池启动无确定高电平 | 原理图 raw-file | 100k 从 VSYS 上拉，开关拉低关机 |
| CRITICAL | U7 VDD 缺少内部 LDO 输出去耦 | TI SLUSBH1C 第 16、18 页 | 增加 1uF，紧邻 pin 5 |
| WARNING | SD 卡无标准上拉、本地去耦且 CD 未接 | Espressif SD pull-up 要求；J3 raw-file | 四路 10k 上拉、10uF+100nF、CD 接 GPIO9 |
| WARNING | CHG/PGOOD/PG 为开漏但缺少上拉 | BQ24074 第 5、35 页；TPS63070 第 3 页 | 增加状态上拉，PGOOD 接 GPIO1 |
| WARNING | GPIO45/46 是 strapping pin | Espressif 模组手册第 13-15 页 | SD_CD→GPIO9，I2S_BCLK→GPIO16 |
| WARNING | U1 10 mm 内无本地去耦 | PCB 几何分析 | 增加 10uF+100nF |
| WARNING | 无调试/电源测试点、无板级贴装基准 | PCB 分析 | 增加 UART、关键电源和 PG 测试点；PCB 阶段补 fiducial |

## 4. 人工覆盖与误报

- MAX98357A 的数字音频输入 VIH 最低 1.3 V，3.3 V ESP32 输出可直接驱动；分析器报告的 5 V/3.3 V level-shifter 错误为误报。
- LS027B7DH01 供电需要 4.8-5.5 V，但 SCLK/SI/SCS/DISP/EXTCOMIN 的 VIH 最低 2.7 V，3.3 V GPIO 可直接驱动；不需要额外电平转换。
- BQ24074 TMR 接定时电阻到地是官方用法，不是缺少上拉。
- 0.15 mm 普通信号线满足嘉立创四层标准能力，不按电流容量告警盲目加宽；高速时钟则按回流和串扰单独优化。
- J1 与 SW1 是板边接口，courtyard 越出板框是安装意图；需以外壳和装配图复核，不是自动判废。
- Gerber 铜层有效图形宽度不同来自局部铜和天线禁布区；Gerber、四铜层、阻焊、板框、PTH/NPTH 文件实际齐全。

## 5. PCB 与 EMC 整改

- SD_DAT0、SD_CLK、I2S_BCLK、I2S_LRCLK、LCD_SCLK 必须保持连续 GND 参考面；禁止跨天线净空或内层分割。
- 每个高速信号换层 via 附近 1 mm 内放 GND 回流 via，优先 LCD_SCLK、PDM_CLK、I2S_BCLK/LRCLK。
- U3/U4 输入电容、功率管和电感形成最短热环；U4 当前启发式热环约 25 mm2，是下一版布局上限而非通过目标。
- 清除 C10/C11/C14/C20/R7 焊盘内未封孔 via，避免回流焊吸锡；U2 热焊盘增加有效导热孔。
- F/B 两面根据实际 PCBA 面补局部 fiducial；若仅底面核心 SMT，至少底面三点或由拼板全局 fiducial 覆盖。

## 6. 采购与验证缺口

- 已缓存 11 个关键器件数据手册和 LCSC 来源记录，但被动器件、分流器、开关、JST、电池、扬声器、滚轮子板仍未 100% 冻结。
- C473397 当前来源库存为零，BQ27441 必须重新确认可贴装库存或选择经过完整引脚/固件审查的替代料。
- J2 的 C324723 返回订货后缀 `(50)`，设计文本写 `(99)`；需确认仅包装数量差异后再冻结。
- 屏幕 FPC 接点朝向、插入面和 pin 1 必须用用户手中实物与连接器样品做通断验证。
- 最终“可投板”证据必须同时包括：关键 pinout 数据手册表、ERC=0、DRC=0、unconnected=0、Gerber/钻孔检查、BOM/CPL parity、嘉立创重新导入截图和人工装配方向复核。
