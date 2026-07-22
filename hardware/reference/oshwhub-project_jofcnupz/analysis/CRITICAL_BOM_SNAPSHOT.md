# 关键器件供应与装配快照

查询日期：2026-07-22（Asia/Shanghai）  
适用对象：OSHWHub `project_jofcnupz` 参考工程，不是 Cadenza 冻结 BOM。  
结论置信度：器件身份来自参考工程 BOM/原理图，较高；库存和嘉立创基础/扩展分类是时点数据，订板前必须重查。

## 结论

- 没有发现“核心 IC 已经明显买不到、所以参考板不可复用”的证据。ESP32-S3 模组、IP5306、TLV62568、CH340C、MAX98357A 和 ME6210 在本次查询中都有现货。
- 本报告覆盖的参考料在当前 JLC/LCSC 查询结果中全部是 **Extended（扩展库）**，不是 Basic（基础库）。这主要影响贴装换料费和备料管理，不等于不能生产。
- 现阶段最需要冻结实物的三项是 **USB-C 座、麦克风、显示 FPC 座**。它们都存在版本或机械匹配问题，不能只按名字或针数替换。
- ST7789 的 12-pin FPC 座只作为参考板证据保留；Cadenza 的 Sharp LS027B7DH01 是 10-pin，必须另选并核对触点方向、Pin 1 和 FPC 插入方向。
- 麦克风不是 L1 的必要功能。参考原理图/BOM 是 ICS-43434，项目页面又称兼容 ZTS6672；在封装、声孔方向和真实装机料号确认前，建议保持可选/DNP。

## 口径与限制

- `库存` 和 `Basic/Extended` 来自 [jlcsearch 社区接口](https://jlcsearch.tscircuit.com/api/search?q=ESP32-S3-WROOM-1-N16R8&limit=8&full=true)，它聚合 LCSC/JLCPCB 料库，查询时间为 2026-07-22。官方 JLCPCB 实时 API 需要单独授权，因此下单前还要在 [JLCPCB Parts](https://jlcpcb.com/parts/componentSearch) 用 C 编号复核。
- `生命周期` 优先使用制造商官方页面。没有公开生命周期字段的器件，写“待验证”，不根据库存量猜测 Active/EOL。
- `封装` 同时核对参考 BOM 的 KiCad footprint 与当前料库结果；连接器还必须拿样品和尺寸图做机械核对。
- 库存会变化；本表只能证明查询当时可采购，不能替代正式采购冻结。

## 关键 IC

| 参考位号 | 参考 MPN / LCSC | 参考封装 | 2026-07-22 库存 | JLC 分类 | 生命周期证据 | 继承/替代判断 |
|---|---|---|---:|---|---|---|
| U4 | `ESP32-S3-WROOM-1-N16R8` / [C2913202](https://www.lcsc.com/product-detail/C2913202.html) | 模组 SMD，约 25.5×18 mm；参考专用 WROOM-1 焊盘 | 21,608 | Extended | Espressif 仍在 2026-03-02 发布的 [WROOM-1/1U v1.8 数据手册](https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf)中列出该系列和 N16R8；正式生命周期字段待验证 | **优先原样继承。** `1U` 外置天线版 C3013946 外形和天线要求不同，不是无条件替代料。 |
| U1 | `IP5306` / [C181692](https://www.lcsc.com/product-detail/C181692.html) | ESOP-8，带散热焊盘 | 29,598 | Extended | 制造商正式生命周期字段未取得，待验证 | **可作为参考基线继承。** `IP5306-LC`、`IP5306-I2C` 名称相近但功能/控制方式可能不同，不列为自动替代；低负载自动关机和 KEY 行为仍需实测。 |
| U2 | `TLV62568DBVR` / [C163219](https://www.lcsc.com/product-detail/C163219.html) | SOT-23-5 (DBV) | 2,526 | Extended | TI 官方将 [TLV62568DBVR](https://www.ti.com/product/TLV62568/part-details/TLV62568DBVR) 标为 **ACTIVE / Production** | **可继承。** 当前另有 `TLV62568DBVR-TP` C52205266（3,000），但制造商身份和后缀含义未完全确认，不能直接当第二来源。 |
| U3 | `CH340C` / [C84681](https://www.lcsc.com/product-detail/C84681.html) | SOP-16，1.27 mm pitch | 1,665；同名 C7464026 为 40,915 | Extended | WCH 仍有 [CH340 产品页](https://www.wch-ic.com/products/CH340.html)，但页面未给正式生命周期字段 | **L1 可继承参考下载电路。** 两个同名 C 编号的制造商/批次差异待验证；选择前核对顶标、数据手册和 SOP-16 尺寸。L2 是否改原生 USB 另行决策。 |
| U5 | `MAX98357AETE+T` / [C910544](https://www.lcsc.com/product-detail/C910544.html) | TQFN-16-EP，3×3 mm，0.5 mm pitch | 15,884 | Extended | ADI 官方标为 [PRODUCTION](https://www.analog.com/en/products/max98357a.html)；官网同时提示新设计可考虑 MAX98360/MAX98365 | **可继承已验证音频拓扑。** 新型号不是已证明的 pin-to-pin 替代，不为追新而改。封装底部散热焊盘必须保留。 |
| U6 | `ME6210A33M3G` / [C236680](https://www.lcsc.com/product-detail/C236680.html) | SOT-23-3 | 21,848 | Extended | 制造商公开生命周期字段待验证 | **暂可继承。** 它属于麦克风局部 3.3 V 供电；若麦克风 DNP，应一起检查是否可 DNP，不能孤立删除。 |

补充：TLV62568 的 2.2 µH 电感 `FNR3015S2R2MT` / C167747 当前库存 148,641；MAX98357A 输出/电源相关 1 µH `MLP2012S1R0MT0S1` / C383371 当前库存 3,369。两者均为 Extended。替代电感不能只看电感值，还要核对额定电流、饱和电流、直流电阻和尺寸，当前替代料均为“待验证”。

## 连接器、开关与人机件

| 功能 / 位号 | 参考料 / LCSC | 参考封装或机械信息 | 2026-07-22 库存 | JLC 分类 | 状态与处理 |
|---|---|---|---:|---|---|
| microSD / CARD1 | `TF PUSH` / [C393941](https://www.lcsc.com/product-detail/C393941.html) | 自弹式 SMD，参考 footprint `TF-SMD_TF-PUSH`；料库只给笼统 `SMD` | 134,113 | Extended | 有货，但 MPN 是泛称。**只能在尺寸图、焊盘、卡插入方向和壳体开口逐项一致后继承。** 生命周期待验证。 |
| USB-C / USB1 | BOM：`HX-TYPE-C 16P 180 LC-B H15.0` / [C49261588](https://www.lcsc.com/product-detail/C49261588.html) | 16P 直插/沉板类，参考专用 footprint；高度对外壳关键 | 349 | Extended | **版本冲突：** 原理图文字写“实际使用 C49261586”，但本次 C49261586 查无结果，导入 BOM 是 C49261588。必须以原作者实物、尺寸图或已装配样板确认；不可自动继承采购号。 |
| ST7789 FPC / FPC1（仅参考） | `FPC-05F-12PH20` / [C2856799](https://www.lcsc.com/product-detail/C2856799.html) | 12P、0.5 mm、卧贴 | 62,789 | Extended | 参考 ST7789 可用证据；**Cadenza 不继承此器件**。Sharp LS027B7DH01 是 10P，需重新选型并实物验证上下接点、Pin 1、锁扣和高度。 |
| I2S 麦克风 / U7 | BOM：`ICS-43434` / [C5656610](https://www.lcsc.com/product-detail/C5656610.html) | 本地 footprint 约 3.5×2.7 mm、6P、0.9 mm pitch、底部声孔 | 4,306 | Extended | 项目页面称兼容 `ZTS6672 / ICS-43434`，但 ZTS6672 本次无结果，且两者封装/声孔/引脚兼容证据不足。**建议 L1 DNP；若保留再独立验证。** |
| BOOT、RESET | `TS36CA-0.6` / [C2681481](https://www.lcsc.com/product-detail/C2681481.html) | SMD 轻触，参考 footprint `SW-SMD_TS36CA` | 9,831 | Extended | 当前可采购；正式生命周期待验证。可继承，但壳体是否要暴露 RESET/BOOT 另定。 |
| 6 个游戏键 / KEY1…6 | `HX TS665WS 200gf` / [C5143657](https://www.lcsc.com/product-detail/C5143657.html) | SMD 6×6 mm，参考高度 5 mm | 4,161 | Extended | 当前可采购；按压力/高度决定手感和壳体公差。L2 若改为编码器/新按键布局，不应把 footprint 当通用 6×6 键。 |
| 原电源滑动开关 / SW1 | `SK12D07VG4-BF` / [C49451756](https://www.lcsc.com/product-detail/C49451756.html) | 直插锁存滑动开关 | 6,165 | Extended | 参考板的硬断/通电开关。Cadenza 需要短按锁屏、长按关机的瞬时键，**功能上不原样继承**；可保留其电源路径作为对照。 |
| 摇杆/多向键 / SW2 | `10*10*9-6P WX` / [C2858290](https://www.lcsc.com/product-detail/C2858290.html) | SMD 6P，约 10×10×9 mm | 1,996 | Extended | 泛称 MPN，机械风险高。Cadenza 计划 EC12 编码器，**不作为等价替代**；只保留布局/交互参考。 |
| 扬声器座 / CN1 | `HC-1.25-2PWT` / [C2845379](https://www.lcsc.com/product-detail/C2845379.html) | 1.25 mm 卧贴 2P | 207,413 | Extended | 可采购。必须同时锁定配套线端、极性、线长和扬声器腔体；只买板端座不构成完整 BOM。 |
| 电池/外接座 / H1 | `WAFER-PH2.0-2PWB` / [C3029440](https://www.lcsc.com/product-detail/C3029440.html) | 2.0 mm 卧贴 2P | 48,843 | Extended | 可采购。参考导入中 H1 footprint 字段缺失/不一致，且 JST-PH 类连接器克隆料极性方向常不同；必须用实物线束确认正负极。 |

## 替代料策略

### 可以先按原料保留

- ESP32-S3-WROOM-1-N16R8、TLV62568DBVR、MAX98357AETE+T：当前有明确精确 MPN、匹配封装和制造商资料。
- IP5306、CH340C：参考样机已经提供了更强的系统级证据，L1 先保持电路与精确料号；采购前补正式批次/制造商核验。
- microSD、扬声器座、BOOT/RESET：可保留 footprint 作为基线，但连接器仍需实物尺寸确认。

### 不能自动替代

- ESP32-S3-WROOM-1U：需要外置天线与不同 keepout/机构设计。
- IP5306-LC、IP5306-I2C：控制方式或默认行为可能不同。
- TLV62568DBVR-TP：当前查询无法证明它是 TI 原厂完全等价订购后缀。
- MAX98360/MAX98365：是新设计候选，不是已经证明的本板 drop-in 替代。
- 任意“同为 16P”的 USB-C 座、任意“同为 0.5 mm”的 FPC 座、任意“6P I2S 麦克风”：机械和引脚差异足以让 PCB 无法装配。

## 下单前必须重查

1. 在 JLCPCB PCBA 页面逐个输入最终 C 编号，确认仍可贴装、库存、最低贴装量、扩展库费用和器件所在仓。
2. 对 USB-C、microSD、Sharp 10P FPC、麦克风、EC12、电源键、扬声器座和电池座保存**制造商尺寸图**，逐一对照 KiCad footprint。
3. 对所有带方向器件在 BOM/CPL 上传预览中确认 Pin 1 和旋转：ESP32 模组、IP5306、TLV62568、CH340C、MAX98357A、USB-C、microSD、FPC、LED、二极管/晶体管。
4. 先决定 L1 是否 DNP 麦克风；若 DNP，连同 U6 及其局部去耦一起形成明确的装配变体。
5. 先用实物确认 USB1 到底是 C49261586 还是 C49261588；在确认前不把任一编号写入 Cadenza 冻结 BOM。
6. Sharp 屏幕连接器必须以屏幕数据手册和用户已购转接板实测为准；参考板的 12P ST7789 连接器不进入 Cadenza BOM。

## 资料来源

- 参考工程 BOM：`kicad-import-smoke-test/gui-full-import/imported-schematic-bom.csv`
- 参考原理图源：`epro-v2-unpacked/SHEET/f9db18ef962b5489/3.esch`
- 参考项目页面（麦克风兼容声明、USB/装配备注）：<https://oshwhub.com/chaeng/project_jofcnupz>
- LCSC/JLC 库查询入口：<https://jlcsearch.tscircuit.com/api/search>；逐项产品页已链接在表中
- JLCPCB 官方器件库复核入口：<https://jlcpcb.com/parts/componentSearch>
- Espressif WROOM-1/1U 官方数据手册：<https://documentation.espressif.com/esp32-s3-wroom-1_wroom-1u_datasheet_en.pdf>
- TI TLV62568DBVR 官方状态：<https://www.ti.com/product/TLV62568/part-details/TLV62568DBVR>
- WCH CH340 官方产品页：<https://www.wch-ic.com/products/CH340.html>
- ADI MAX98357A 官方产品页：<https://www.analog.com/en/products/max98357a.html>

