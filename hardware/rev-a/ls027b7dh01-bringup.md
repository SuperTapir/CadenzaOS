# LS027B7DH01 直接接入与台架点亮规范

## 结论

屏幕使用原生 10-pin、0.5 mm FPC 尾巴直接接入 Rev A 主板，不需要也不采用第三方转接板。主板使用可兼容上下接点的 `Hirose FH34SRJ-10-0.5SH` 或经过嘉立创封装/贴装确认的等效连接器。

屏幕实物照片确认 FPC 从模块单边引出。连接器的实际接触面与 pin 1 朝向必须以 Sharp `LS027B7DH01` 规格书的 pin-assignment 图和 PCB 丝印双重确认，不能仅凭照片推断。

## 原生引脚表

| FPC pin | 名称 | Rev A 连接 | 说明 |
| --- | --- | --- | --- |
| 1 | SCLK | ESP32-S3 SPI clock GPIO | 串行时钟；空闲保持低电平 |
| 2 | SI | ESP32-S3 SPI MOSI GPIO | 串行数据；空闲保持低电平 |
| 3 | SCS | ESP32-S3 chip-select GPIO | 片选；无通信时必须保持低电平 |
| 4 | EXTCOMIN | ESP32-S3 定时 GPIO | 外部 COM 反转脉冲 |
| 5 | DISP | ESP32-S3 display-enable GPIO | 高电平显示内存内容；按规格书加最小 560 pF 到 VSS |
| 6 | VDDA | `LCD_5V` | 模拟显示电源；连接器旁 1.0 uF 至 VSSA |
| 7 | VDD | `LCD_5V` | 数字显示电源；连接器旁 1.0 uF 至 VSS |
| 8 | EXTMODE | `LCD_5V` | 固定高电平，选择外部 EXTCOMIN 模式；上电后不得改变 |
| 9 | VSS | GND | 数字地 |
| 10 | VSSA | GND | 模拟地；在显示连接器附近与数字地低阻连接 |

## 电源与信号约束

- `VDDA` 与 `VDD` 的工作范围为 4.8-5.5V；Rev A 目标为受控的 `LCD_5V`。
- `SCLK`、`SI`、`SCS`、`EXTCOMIN`、`DISP` 的高电平由 3.3V ESP32-S3 GPIO 驱动，符合屏幕约 3V 输入高电平要求。
- `EXTMODE` 固定接 5V 后，COM 极性由 `EXTCOMIN` 的定时上升沿反转；固件必须按规格书保持稳定周期。
- 无数据发送时，`SCLK`、`SI`、`SCS` 必须为低电平，尤其 `SCS` 不得保持高电平。
- 在显示 FPC 插入方向、5V 电源、地连续性和 GPIO 空闲电平被万用表确认前，不得上电。

## 首次点亮顺序

1. 不插屏，确认 `LCD_5V` 在 4.8-5.5V，且 3.3V GPIO 不会把 5V 带回 ESP32-S3。
2. 断电后插屏，检查 FPC 锁扣、pin 1 丝印和 FPC 弯折半径。
3. 上电后保持 `SCS/SCLK/SI` 低，拉高 `DISP`，以定时 `EXTCOMIN` 启动 COM 反转。
4. 先发送全白、全黑、棋盘格和边框图案；记录显示方向、反相、闪烁与电流。
5. 再验证全屏刷新、局部刷新和静态保持；每项记录波形和电流，作为 Rev B 对比基线。

## 机械约束

- 玻璃模块由外壳压框和软垫限位，FPC 不承受拉力或扭力。
- FPC 仅在距玻璃边缘 0.8-6.0 mm 的推荐区域弯折，内弯半径不得小于 R0.45 mm。
- FPC 不得向显示正面偏光片方向折弯，重复 180 度折弯不得超过三次。
