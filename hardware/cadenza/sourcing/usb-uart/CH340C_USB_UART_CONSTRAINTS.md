# CH340C USB-UART 约束说明

查询时间：2026-07-22 08:45:18 +08:00（2026-07-22T00:45:18Z）

## 结论

CH340C 是 USB 2.0 **全速（Full-Speed，12 Mbit/s）** USB-UART 芯片；不要把串口最高 2 Mbit/s 与 USB 总线速度混为一谈。SOP-16 封装的 `UD+` 是 5 脚，`UD-` 是 6 脚。WCH 明确建议二者直接接 USB 数据线，**不要额外串联电阻**；芯片已内置 USB 上拉和时钟。

当前候选导出的 netlist 中，U3 的 5/6 脚分别接 `/D+`、`/D-`，4/16 脚同接 `3V3M`，1 脚接 GND。这与 WCH 的 3.3 V 供电和 SOP-16 USB 引脚定义在拓扑上相符；这只是 netlist 观察，不代表 PCB 阻抗、走线或实板已经验证。

## 已锁定官方资料

- WCH 官方产品页：<https://www.wch.cn/products/CH340.html>
- WCH 官方产品 API：<https://api1.wch.cn/api/official/website/articles/getArticle?alias=CH340.html>
- WCH 官方手册页：<https://www.wch.cn/downloads/CH340DS1_PDF.html>
- WCH 官方文件 API 记录：`CH340DS1.PDF`，版本 `3.4`，上传日期 `2025-03-03`，适用范围 `CH340`，标签包含 CH340C。
- USB-IF USB 2.0 规范页：<https://www.usb.org/document-library/usb-20-specification>

本地英文 PDF 是 LCSC 托管的 WCH V3C 副本：<https://datasheet.lcsc.com/datasheet/pdf/e2f14e51aaa60c793f1f0cbc8a5d5faa.pdf?productCode=C84681>。其标题、作者、正文和页脚均标注 WCH / `wch-ic.com`，但它不是本次从 WCH 官网直下的 v3.4 文件，不能用其哈希代表 WCH v3.4。

## WCH 明确说明

以下是厂家一手资料可以直接支持的结论：

| 项目 | 厂家明确内容 | L1 约束 |
|---|---|---|
| USB 速度等级 | Full-Speed USB device，兼容 USB 2.0 | 按 USB Full-Speed 设计；总线速率 12 Mbit/s |
| UART 数据率 | 50 bit/s 至 2 Mbit/s | 与 USB 速度分开记录 |
| SOP-16 USB 引脚 | 5 = UD+，6 = UD- | 连接器 D+ 到 5 脚、D- 到 6 脚，禁止互换 |
| USB 串联电阻 | UD+/UD- 直接接 USB 总线，建议不加额外串联电阻 | 不沿用常见 MCU 的 22 Ω/27 Ω 经验值 |
| USB 上拉 | 芯片内置 USB pull-up | 不再外加 D+ 上拉 |
| 时钟 | CH340C 内置时钟 | 不需要 12 MHz 晶振与两只振荡电容；SOP-16 7 脚 NC 必须悬空 |
| 5 V 供电 | VCC 输入 5 V；V3 只接 0.1 µF 去耦到地 | 适用于选择 5 V 供电时 |
| 3.3 V 供电 | V3 与 VCC 相连并输入 3.3 V；相连电路不得超过 3.3 V | 当前 netlist 观察为 4/16 脚同接 `3V3M` |
| 电源去耦 | VCC 外接 0.1 µF；参考应用的 V3、外部电源去耦均为 0.1 µF | 电容紧靠对应引脚，地回路短 |
| 地 | GND 直接接 USB 总线地 | USB 返回路径必须连续、低阻抗 |
| WCH PCB 提示 | 去耦靠近芯片；D+/D- 靠近并行；两侧尽量有地线或地铜以减小干扰 | 作为 PCB 评审硬门禁 |
| 未用引脚 | 未用 I/O 可悬空；CH340C 7 脚是 NC 且必须悬空 | 不给 NC7 接铜、测试点或地 |

批次相关事项：WCH 说明“批号以 4 开头”的部分 CH340C 具有额外的 5 V 耐受/防倒灌能力，8 脚的第二 DTR# 功能也受更具体批号条件限制。当前 BOM 未锁批次，因此 **不得依赖这些批次特性**。

## 通用 USB 保守推导

以下不是 WCH 对本板给出的精确数值，而是为了降低首板风险采用的通用 USB 设计规则：

- 把 D+/D- 作为一个差分对一起布线，采用约 90 Ω 差分阻抗目标。
- 两线同层、同参考平面、尽量同长；不要在其中一根线上单独绕路制造长度补偿。
- 从 USB 连接器到 CH340C 保持短、直，避免支路、测试点长 stub 和不必要的过孔。
- 下方保持连续 GND 参考平面，不跨平面缝隙；连接器屏蔽、ESD 回流和数字地的连接方式需在整板上复核。
- 若保留 ESD/共模器件，占位与选型要按 Full-Speed USB 的寄生电容和封装验证；WCH 的“不串电阻”不等于“禁止低电容并联 TVS”。
- D+/D- 从连接器到芯片的极性、连续性和无短路必须在 Gerber 回读与首板万用表检查中再次确认。

这些规则是保守起点，不构成 USB-IF 认证或信号完整性仿真结论。

## 待嘉立创叠层/阻抗计算

在板层、板厚、铜厚、走线层、参考平面距离和阻焊参数未锁定前，以下内容全部保持 `待验证`：

- 90 Ω 差分阻抗对应的实际线宽和线间距。
- D+/D- 到参考平面的实际介质厚度和材料参数。
- 是否需要嘉立创阻抗控制服务、选择哪一个官方叠层编号。
- 过孔结构、换层位置及其对阻抗的影响。
- 允许的长度差、总长度和连接器/ESD 器件造成的不连续性。

因此目前不能凭“差分对规则已设置”或 DRC 为 0 宣称 USB 走线可靠。

## 当前候选只读观察

来源：`hardware/cadenza/derived/l1-candidate/candidate.netlist.xml`，SHA-256 `40a5f97f77034897099e21b31176b486de2ea824a5a7987637af299a406d32db`。

| U3 脚 | 当前网络 | WCH 定义 | 只读判断 |
|---|---|---|---|
| 1 | GND | GND | 拓扑相符 |
| 4 | 3V3M | V3 | 与 3.3 V 供电方式相符 |
| 5 | /D+ | UD+ | 极性相符 |
| 6 | /D- | UD- | 极性相符 |
| 7 | unconnected | NC | 相符 |
| 16 | 3V3M | VCC | 与 3.3 V 供电方式相符 |

尚未由本任务确认：去耦实际数量和距离、D+/D- 物理走线、参考平面、USB-C 端映射、TVS、阻抗、回流路径、嘉立创回导结果和实板枚举。

## 状态

- `official_datasheet_record_locked=true`
- `official_datasheet_version=3.4`
- `official_pdf_direct_downloaded=false`
- `schematic_modified=false`
- `pcb_modified=false`
- `routing_frozen=false`
- `stackup_calculated=false`
- `production_ready=false`
