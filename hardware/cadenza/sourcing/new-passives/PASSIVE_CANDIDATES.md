# Cadenza 新增被动件候选

状态：**可写入候选原理图，尚不是下单冻结 BOM。** 采购库存与嘉立创基础库属性是
2026-07-22 的快照，下单前必须重新查询。

## 选择结果

| 用途 | 数值/规格 | 候选 MPN | LCSC | JLCPCB | 快照库存 |
|---|---|---|---|---|---:|
| Sharp DISP、VDDA 去耦 | 100 nF ±10%，50 V，X7R，0805 | CC0805KRX7R9BB104 | C49678 | Basic | 15,768,009 |
| Sharp VDD 去耦 | 1 µF ±10%，50 V，X7R，0805 | CL21B105KBFNNNE | C28323 | Basic | 5,331,021 |
| USB D+/D− 串联 | 22 Ω ±1%，0805 | 0805W8F220JT5E | C17561 | Basic | 1,236,224 |
| Power/Lock 门极串联、L2 输入串联 | 100 Ω ±1%，0805 | 0805W8F1000T5E | C17408 | Basic | 5,280,115 |
| Power/Lock 感知串联 | 1 kΩ ±1%，0805 | 0805W8F1001T5E | C17513 | Basic | 7,961,494 |
| Power/Lock/L2 输入上拉 | 10 kΩ ±1%，0805 | 0805W8F1002T5E | C17414 | Basic | 15,457,503 |
| Power/Lock 下拉、VBUS 分压下臂 | 100 kΩ ±1%，0805 | 0805W8F1003T5E | C149504 | Basic | 1,778,254 |
| VBUS 分压上臂，单件方案 | 75 kΩ ±1%，0805 | 0805W8F7502T5E | C17819 | **Extended** | 103,337 |
| VBUS 分压上臂，Basic 方案 | 150 kΩ ±1%，0805，两个并联 | 0805W8F1503T5E | C17470 | Basic | 506,046 |

## 采用建议

- L1 的两颗 100 nF 和一颗 1 µF 可采用上表候选。两种电容均有制造商参数证据，
  但必须按 Sharp 数据手册要求放在相应 FPC 电源脚附近，不能只满足 BOM 数值。
- Power/Lock 与 L2 输入尽量复用同一组 Basic 0805 电阻，降低扩展料种数。
- USB VBUS 检测优先预留 `2 × 150 kΩ 并联` 作为 75 kΩ 上臂，并配一颗 100 kΩ
  下臂。它在 5 V 时给出约 2.86 V，且三颗都是 Basic；代价是多占一个 0805 位置。
  若 PCB 空间不足，才改用单颗 C17819 Extended。两种装法不可同时装。
- USB 数据线的两颗 22 Ω 必须同型号、对称放置并靠近 ESP32-S3；“同一包装”不等于
  已经完成差分走线验证。

## 证据边界

- Yageo 官方规格确认 `CC0805KRX7R9BB104` 为 100 nF、±10%、50 V、X7R、0805。
- Samsung Electro-Mechanics 官方产品页确认 `CL21B105KBFNNN#` 系列中的
  `CL21B105KBFNNNE` 为量产状态、1 µF、±10%、50 V、X7R、0805。
- 电阻采用参考工程已经使用的 UNI-ROYAL 0805W8F 系列；具体阻值由完整 MPN 编码和
  LCSC 条目交叉确认。候选只用于低功耗逻辑上拉、分压和串联，不用于功率耗散路径。
- `basic`、库存和价格来自 jlcsearch 社区 API，不是嘉立创的锁库存承诺；生产前必须在
  JLCPCB BOM 匹配页面再次确认封装、基础/扩展属性和可用库存。

## 冻结前检查

- [ ] 从完成后的 KiCad 原理图导出 BOM，而不是手工把本表当生产 BOM。
- [ ] 确认 0805 焊盘与实际器件高度、屏幕后压框和外壳净空兼容。
- [ ] 对照 Sharp 数据手册复核三颗显示电容的数值、连接点和摆放距离。
- [ ] 选择 VBUS 上臂的单件或双件装法，并在原理图中设置互斥/DNP。
- [ ] 下单当天重新查询 LCSC/JLCPCB，并保存 BOM 匹配截图或导出结果。

## 来源

- Yageo：<https://www.yageogroup.com/download/specsheet/CC0805KRX7R9BB104>
- Samsung Electro-Mechanics：<https://product.samsungsem.com/mlcc/CL21B105KBFNNN.do>
- LCSC/JLCPCB 搜索快照：`passive-candidates.json`

