# LS027B7DH01 屏幕接口冻结证据

> 结论状态：条件冻结。以下电气和机械定义适用于 Sharp `LS027B7DH01`。必须在用户手中屏幕的玻璃/FPC/包装标签上确认完整型号后，才能把条件冻结改为正式冻结。

## 1. 型号与基础参数

Sharp 规格书 `LD-28305A` 明确给出：

| 项目 | 规格 |
|---|---|
| 型号 | LS027B7DH01 |
| 类型 | 2.7 英寸单色半反透 Memory LCD |
| 分辨率 | 400 x 240 |
| 可视区 | 58.8 x 35.28 mm |
| 像素间距 | 0.147 x 0.147 mm |
| 模组外形 | 62.8 x 42.82 x 1.64 mm，不含突出部分 |
| 典型重量 | 9.9 g |
| 接口 | 10 针串行 FPC |

证据：[Sharp LS027B7DH01 规格书 LD-28305A](https://pages.azumotech.com/hubfs/Sharp%20Spec%20Sheets/2.7_LS027B7DH01%20Spec%20%28LD-28305A%29.02b53ab6.pdf)、[Sharp 器件目录](https://global.sharp/products/device/catalog/pdf/sharp_device202109_e.pdf)。

## 2. FPC 针序

规格书表 4-1 定义了从 Pin 1 到 Pin 10 的顺序：

| Pin | 名称 | 方向 | Cadenza D1 网络/处理 |
|---:|---|---|---|
| 1 | SCLK | 输入 | `LCD_SCLK` |
| 2 | SI | 输入 | `LCD_SI` |
| 3 | SCS | 输入 | `LCD_SCS` |
| 4 | EXTCOMIN | 输入 | `LCD_EXTCOMIN` |
| 5 | DISP | 输入 | `LCD_DISP` |
| 6 | VDDA | 电源 | 受控 5 V LCD 电源 |
| 7 | VDD | 电源 | 受控 5 V LCD 电源 |
| 8 | EXTMODE | 输入 | 使用外部 COM 翻转时接 VDD；使用串行 flag 时接 VSS。Rev A 采用哪种方式必须与固件一致 |
| 9 | VSS | 地 | GND |
| 10 | VSSA | 地 | GND |

重要电气条件：

- `VDDA` 与 `VDD` 推荐工作范围均为 4.8-5.5 V，典型值 5.0 V。
- 输入高电平推荐约 3.0 V，最低 `VIH` 为 2.70 V，因此 ESP32-S3 的 3.3 V GPIO 可直接驱动逻辑输入。
- `VDD` 和 `VDDA` 应同时上升，或 `VDD` 先上升；关断时同时下降，或 `VDD` 先下降。
- `DISP` 与 `EXTCOMIN` 启动后，到第一次 `SCS` 之间至少保留 30 us。
- `DISP-VSS` 推荐 560 pF；`VDDA-VSS` 至少 0.1 uF；`VDD-VSS` 至少 1 uF，均应靠近 FPC 连接器。
- 屏幕自身供电不是 3.3 V。`LCD_5V_EN` 只负责控制 5 V 电源路径，不能替代正确的 5 V 时序与去耦。

## 3. 连接器条件冻结

Sharp 推荐 Panasonic `AYF531035`。原厂规格与立创映射：

| 项目 | 规格 |
|---|---|
| MPN | AYF531035 |
| LCSC | C425133 |
| 针数/间距 | 10 pin / 0.5 mm |
| FPC 厚度 | 0.30 mm |
| 接触方向 | Panasonic 原厂定义为上下双接点 |
| 锁扣 | Back lock |
| 安装 | SMT，卧式 |
| 本体高度 | 1.00 mm |
| 本体长度 | 7.00 mm |
| 额定电流 | 0.5 A/contact |
| 插拔寿命 | 20 次 |

证据：[Panasonic AYF531035 原厂页面](https://industry.panasonic.com/global/en/products/control/connector/fpc-ffc/number/ayf531035)、[LCSC C425133](https://www.lcsc.com/product-detail/FFC-FPC-Flat-Flexible-Connector-Assemblies_PANASONIC-AYF531035_C425133.html)。

注意：LCSC 页面摘要把接触类型简化为 `Bottom Contact`，而 Panasonic 原厂数据为上下双接点。封装和采购验收必须以 Panasonic 图纸为准，不得用立创摘要反推焊盘方向。

## 4. FPC 与外壳约束

- 推荐弯折区距玻璃边缘 0.8-6.0 mm。
- 最小内弯半径 R0.45 mm。
- 不得向前偏光片方向反折。
- 规格书建议总弯折次数不超过 3 次。
- FPC 不得接触玻璃锐边，也不能靠 FPC 悬挂屏幕。
- 屏幕背面不能承受持续压力；前壳压边只能作用于规格允许的非显示区域。
- FPC 外缘存在金属图形，外壳不能让导电结构与其接触。

因此外壳 CAD 必须包含 FPC 自然弯曲包络，不能只画一条零厚度折线。首轮尺寸壳要使用真实屏幕和真实 `AYF531035` 样件验证锁扣可操作性。

## 5. 正式冻结前的实物检查

- [ ] 屏幕本体或原包装明确标有 `LS027B7DH01`，而不是仅由卖家标题声称。
- [ ] FPC 确认为 10 个触点，节距 0.5 mm，加强板厚 0.30 mm。
- [ ] 用卡尺确认玻璃外形接近 62.8 x 42.82 mm，排除同分辨率不同模组。
- [ ] 确认 FPC 金手指朝向与 PCB 上 `AYF531035` 的 Pin 1 方向。
- [ ] PCB Pin 1-10 与本文针序逐项对照，无镜像、无倒序。
- [ ] `EXTMODE` 硬件绑定位与固件 COM 翻转实现一致。
- [ ] VDDA/VDD 的 5 V、电源时序和三颗推荐去耦在原理图与 PCB 上逐项确认。

完成以上检查后，可以将 LCD 连接器从 `hand/TBD` 移到冻结 BOM；在此之前可先购买 3-5 个 `C425133` 样件做机械试装，但不应直接批量代贴。
