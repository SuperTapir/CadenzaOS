# L1 Sharp LS027B7DH01 面包板验证固件

这是一个与 CadenzaOS 主固件完全隔离的 ESP-IDF 5.5 小工程，只用于验证
`ESP32-S3-DevKitC-1 N16R8 + LS027B7DH01` 的接线、供电和基本显示协议。
顶层 `COMPONENTS` 只包含 `main`，再由组件声明精确拉入 GPIO、LEDC、SPI 与
`esp_timer` 依赖，不使用会连带所有外设的旧 `driver` 聚合组件，也不编译这个
小测试不需要的 Wi-Fi；`sdkconfig.defaults` 还显式关闭了未使用的 TLS 证书包
生成器，避免构建被无关的主机 Python `_csv`/代码签名问题阻断。

它**不能控制显示的 5 V 电源**。首次测试必须先启动固件，让五个信号 GPIO
全部变为低电平，再按串口提示手动插入显示 5 V 跳线。

## 固定测试条件

| 屏幕信号 | ESP32-S3 | 固件条件 |
|---|---:|---|
| SCLK | GPIO39 | SPI mode 0，1 MHz |
| SI | GPIO48 | LSB-first |
| SCS | GPIO47 | 手动控制，满足 3 µs setup / 1 µs hold |
| EXTCOMIN | GPIO14 | LEDC 硬件输出 2 Hz、50% 占空比 |
| DISP | GPIO12 | 完成白屏初始化后才拉高 |

完整的 10-pin 接线、电容和万用表步骤见
[`../../electrical/display/BREADBOARD_BRINGUP.md`](../../electrical/display/BREADBOARD_BRINGUP.md)。
不要根据转接板丝印猜 Pin 1。

## 协议依据

没有根据印象猜测屏幕协议：

- Sharp 原厂 `LCY-1210401A`：
  `hardware/reference/oshwhub-project_jofcnupz/datasheets/LS027B7DH01_Sharp_LCY-1210401A.pdf`。
  采用第 11 页上电顺序、第 12–13 页时序限制、第 14–20 页串行帧、COM 反转和
  行地址定义。这里的页码是 PDF 内印刷页码。
- 本地锁定的 u8g2 源码提交
  `ab9e48b2228351e9476682a70b7f3ee4909cd585`，文件
  `.research/references/u8g2/csrc/u8x8_d_ls013b7dh03.c`。它对
  `LS027B7DH01 400x240` 使用更新命令、逐行地址、50 字节像素、逐行 dummy 和
  末尾 dummy；本工程用 ESP-IDF 的 LSB-first SPI 表达同一线序。

本工程没有复制 u8g2 源文件，只把它作为已经使用该型号的独立交叉证据。
u8g2 代码使用 BSD-2-Clause 许可证。

像素位 `0` 为白、`1` 为黑的依据来自同一锁定 u8g2 版本的两处可追踪行为：
`csrc/u8g2_hvline.c` 明确规定置位表示黑色 ink，而 LS027 驱动把图像字节原样
发送给面板（只反转命令和行地址的位序）。Sharp PDF 的文本抽取没有单独写出
像素高低电平与颜色的对应关系，因此这项仍要用首次实物的全白/全黑结果复核。

每个完整帧在线上是：更新命令 `0x01`、240 组“LSB-first 行地址 + 50 字节
像素 + dummy”，最后再发一个 dummy。单帧共 12,482 字节，1 MHz 下约
100 ms，因此 4 Hz 刷新仍留有约 150 ms 空档。

ESP32-S3 的 LEDC 定时器只有 14-bit 分辨率，不是 20-bit。本工程使用 14-bit
和 `LEDC_AUTO_CLK`；ESP-IDF 5.5 会依次检查 APB、XTAL、RC_FAST，前两者在
2 Hz / 14-bit 下分频超限，最终选择经校准的约 17.5 MHz RC_FAST。其计算分频
参数约 136,719，小于驱动上限 262,143，因此能够输出 2 Hz、50% 占空比。

## 构建

本机项目约定使用官方 ESP-IDF 5.5 checkout：

```sh
cd /Users/tapir/Development/CadenzaOS/hardware/cadenza/validation/l1-display-firmware
source /Users/tapir/Development/esp-idf-v5.5/export.sh
idf.py -B build build
```

烧录和串口监视器（把端口替换为实际值）：

```sh
idf.py -B build -p /dev/cu.usbmodemXXXX flash monitor
```

串口默认 115200 baud。这个工程不是 T-Embed 主固件，因此不要用
`tools/firmware_uac.sh` 烧录它；那个脚本仍只负责现有 T-Embed ESP-IDF 主固件。

### 已完成的静态构建验证（2026-07-22）

- 使用 `/Users/tapir/Development/esp-idf-v5.5`，以 `esp32s3` 为目标，从空构建目录
  完成 523 个构建步骤；加入 EXTCOMIN 运行时频率检查后再次编译，最终应用镜像
  为 `0x361a0` 字节，默认 1 MiB 应用分区
  尚余 79%。
- 构建日志确认未调用本工程不需要的证书包生成器。
- 最终又从空的 `/tmp` 构建目录独立复验一次，使用 ESP-IDF 自己的 Python 3.13
  环境；没有修改系统 Python，项目目录也没有留下 `build/` 或 `sdkconfig`。
- 以上只证明代码能用官方 ESP-IDF 5.5 完整编译。尚未烧录开发板，也尚未用
  示波器/逻辑分析仪或真实屏幕验证波形、像素极性与图案。

固件启动 EXTCOMIN 后会读取 LEDC 的实际配置频率；如果不是恰好 2 Hz，会立即
报错并停止启动，不会把近似频率静默当作成功。50% 占空比由 14-bit 定时器的
`8192/16384` 明确设置。最终仍应在 GPIO14 上实测一个完整周期约 500 ms。

## 首次运行流程

1. 拔掉显示 5 V，只保留已确认的地、信号和三颗数据手册电容。
2. 启动固件。它会先把 GPIO39/48/47/14/12 全部输出低电平。
3. 串口输入 `ARM`。
4. 按提示手动插入显示 5 V，用万用表确认屏幕 pin 6 和 pin 7 对地均为
   4.8–5.5 V。
5. 电压正确后输入 `GO`。固件先在 `DISP=0`、`EXTCOMIN=0` 时写整屏白色，
   再启动 DISP 和 2 Hz EXTCOMIN。
6. 默认依次显示全白、全黑、竖条、棋盘格，各 30 秒、4 Hz 整帧刷新；随后
   停止像素更新并保留 2 Hz EXTCOMIN，进入静态保持。
7. 结束前输入 `shutdown`。固件会写白、等待一秒、拉低 DISP 和 EXTCOMIN；
   串口提示后再手动拔掉显示 5 V。

可用命令：`cycle`、`white`、`black`、`stripes`、`checker`、`hold`、
`status`、`shutdown`、`help`。

## 边界与待验证项

- 固件构建通过只能证明 API、目标和静态协议结构正确，不能代替真实屏幕照片、
  电压、长时间运行和 FPC 方向证据。
- DevKitC-1 v1.0 可能在 GPIO48 上带板载 RGB LED；这可能增加负载或随 SI
  闪烁。请记录开发板版本。最终参考 PCB 不受这颗开发板 LED 影响。
- ESP32-S3 在应用启动前有一小段引脚未由本程序控制的时间。首次或复位测试时，
  必须先拔掉显示 5 V；不要在显示仍供电时按复位键。
- `hold` 只停止像素数据更新，绝不停止 EXTCOMIN。真实静态保持仍需实物验证。
- 此工程不做生产 GPIO、电源时序或 IP5306 低负载行为的最终冻结。
