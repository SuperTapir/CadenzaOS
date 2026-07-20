# CadenzaOS Rev A 关键器件调研记录

状态：候选清单，尚未冻结为可下单 BOM。

## 已确认的显示条件

| 项目 | 结论 | 依据 |
| --- | --- | --- |
| 显示屏 | Sharp LS027B7DH01，400 x 240 单色 Memory LCD | Sharp 规格书 `LD-28X01B` |
| 模块 / 可视区 | 62.8 x 42.82 x 1.64 mm / 58.8 x 35.28 mm | Sharp 规格书 |
| 接口 | 10-pin、0.5 mm FPC；推荐连接器含 Hirose `FH34SRJ-10-0.5SH` | Sharp 规格书 |
| 电源 | VDDA、VDD 为 4.8-5.5V；SCLK、SI、SCS、DISP、EXTCOMIN 输入高电平可由约 3V 逻辑驱动 | Sharp 规格书 |
| 本地去耦 | VDDA-VSSA 与 VDD-VSS 各 1.0 uF；DISP-VSS 最低 560 pF | Sharp 规格书 |
| 结构 | FPC 只在距玻璃 0.8-6.0 mm 区域折弯，内径不小于 R0.45 mm，且不得向前偏光片侧折弯 | Sharp 规格书 |

## Rev A 候选

| 功能 | 候选 | 初步采用理由 | 待确认 |
| --- | --- | --- | --- |
| 主控与无线 | Espressif `ESP32-S3-WROOM-1-N16R8` | 模组带 16 MB Flash、8 MB PSRAM、Wi-Fi 与 BLE，具 I2S、USB OTG、SD/MMC；LCSC/JLC 料号 `C2913202` | 天线净空、嘉立创实际库存与贴装报价 |
| 显示 5V | TI `TPS61023DRLT` | 由单节锂电升至 5V；低负载效率和关断断开能力适合显示电源域；LCSC/JLC 料号 `C1852149` | 显示与扬声器是否共用 5V、实际纹波与 BOM 成本 |
| 充电与电源路径 | TI `BQ24074` | 单节锂电、最大 1.5A、PowerPath、热调节和温度监控 | 嘉立创料号、封装、USB-C 5V 保护与额定充电电流 |
| 麦克风 | TDK InvenSense `ICS-43434` | 底部进音、I2S 24-bit 输出、65 dBA SNR、3.5 x 2.65 x 0.98 mm，适合独立前面拾音孔 | 嘉立创料号与库存；与 ESP32-S3 I2S 时钟和机械开孔的实测 |
| 扬声器功放 | ADI `MAX98357A` TQFN | I2S 直入、2.5-5.5V、5V/4 ohm 可达 3.2W、无需 MCLK，首板容易调试 | 嘉立创料号；扬声器阻抗、额定功率、声腔与 EMI 实测 |
| 用户存储 | microSD 卡座 | 录音容量可替换，可经 USB / 读卡器恢复；ESP32-S3 模组 Flash 只保存设置和小状态 | 卡座型号、侧插/底插方向、防尘与外壳开口 |

## 已确定范围

- Rev A 使用 Wi-Fi 与 Bluetooth LE，不支持经典蓝牙耳机。
- Rev A 不设置 3.5 mm 耳机孔；私密音频留到 Rev B 的无线方案一并决策。
- ESP32-S3 模组 Flash/NVS 保存系统状态；microSD 保存录音和用户内容。
- 显示与扬声器共用 5V 电源的可行性尚未证明；原理图前须决定是否分为两个受控 5V 支路。

## 资料来源

- Sharp `LS027B7DH01` specification `LD-28X01B`，屏幕原始规格。
- Espressif `ESP32-S3-WROOM-1 & WROOM-1U Datasheet v1.8`。
- Espressif ESP-IDF ESP32-S3 Bluetooth LE overview：S3 仅支持 BLE，不支持 Classic Bluetooth。
- TDK InvenSense `ICS-43434` data sheet `DS-000069 v1.2`。
- ADI `MAX98357A/MAX98357B` data sheet Rev. 16。
- TI `TPS61023` data sheet Rev. B。
- TI `BQ2407x` data sheet Rev. N。
