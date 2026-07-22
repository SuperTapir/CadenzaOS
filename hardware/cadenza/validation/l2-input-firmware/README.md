# L2 输入面包板验证固件

这是与 CadenzaOS 主固件完全隔离的 ESP-IDF 5.5 工程，只验证候选的 L2 输入：
左侧 `B`、`Menu`，右侧 EC12 旋转以及编码器中心按压 `A`。它不控制屏幕、音频、
USB 电路或电源开关。

## 先断电接线

每个信号都采用“外部 `10 kΩ` 上拉到开发板 `3V3`、接点动作时接地”的主动低逻辑。
固件同时启用 ESP32 内部上拉作为后备，但内部上拉不能代替外部 `10 kΩ`。

| 功能 | ESP32-S3 | 接线 |
|---|---:|---|
| EC12 A 相 | GPIO15 | 经候选 `1 kΩ` 串阻接 EC12 A；`10 kΩ` 上拉到 3V3 |
| EC12 B 相 | GPIO16 | 经候选 `1 kΩ` 串阻接 EC12 B；`10 kΩ` 上拉到 3V3 |
| EC12 公共端 C | GND | 只接地，不接 3V3 或 5V |
| 编码器中心按压 / A | GPIO17 | 一端经 `1 kΩ` 到 GPIO，另一端接 GND；GPIO 侧 `10 kΩ` 上拉 |
| B 键 | GPIO5 | 一端经 `1 kΩ` 到 GPIO，另一端接 GND；GPIO 侧 `10 kΩ` 上拉 |
| Menu 键 | GPIO8 | 一端经 `1 kΩ` 到 GPIO，另一端接 GND；GPIO 侧 `10 kΩ` 上拉 |

若装候选 `39 nF`，它从 GPIO 一侧（`10 kΩ` 与 `1 kΩ` 的连接点）接到 GND；不确定时
先 DNP，不要把电容串进信号线。首次先只给开发板供电，不接任何输入线，测得 `3V3`
对 GND 约为 3.3 V 后断电，再一次只接这一组输入。任何接线改动都先断 USB 电源。

首次接线不要依据 EC12 外形猜 A/C/B 或中心按键脚。用万用表通断档确认，结果记录到
[`../../physical-evidence/MEASUREMENT_SESSION.md`](../physical-evidence/MEASUREMENT_SESSION.md)。
如果暂时没有 `1 kΩ` 串阻和可选 `39 nF` 电容，可以先按面包板验证目的记录实际配置；
最终 PCB 数值不能据此冻结。

## 固件行为

- GPIO15/16 使用双边沿中断，将 A/B 同时状态送到队列；解码任务不在 ISR 中执行 UI。
- Gray-code 状态机只接受相邻状态，跨越两个 bit 的非法跳变会计数、重新同步并清除
  未完成步数，不会凭空产生一格。
- 合法状态抖动可正反抵消。队列溢出单独计数，不能被“看起来能转”掩盖。
- 三个按键以 1 ms 周期采样，默认连续稳定 20 ms 才改变状态；默认 800 ms 产生一次
  `LONG`。释放长按不会再产生 `SHORT`。
- 每秒输出每路 raw/stable 状态、边沿、事件和错误统计。

编码器的 `enc_raw` 是读取当下 GPIO 的 A/B，`enc_stable` 是任务已经按顺序处理到的
Gray 状态；两者在队列尚未清空的瞬间可以短暂不同，这正是需要观察的诊断信息。

串口命令（115200 baud）：

```text
status
reset
reverse 0|1
cpd 1..4
events 0|1
help
```

`reverse` 只改变报告方向；`cpd` 表示每卡点包含几个合法 Gray 状态变化。两者都有
Kconfig 默认值，也能在运行中调整。因为真实 EC12 尚未测量，本工程不声称哪个方向是
顺时针，也不默认一格是 1、2 或 4 个状态变化。

`events` 控制是否逐条打印旋钮和按钮事件，默认关闭；每秒统计始终保留。逐格串口
`printf` 本身会占用 CPU 和 UART，可能造成高速旋转时的仪器效应，所以执行 500 卡点
压力测试必须使用 `events 0`，只看周期统计与最终 `status`。

## 构建与静态测试

```sh
cd /Users/tapir/Development/CadenzaOS/hardware/cadenza/validation/l2-input-firmware
source /Users/tapir/Development/esp-idf-v5.5/export.sh
idf.py -B build build
```

只测与硬件无关的状态机和消抖逻辑：

```sh
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_input_logic.c main/input_logic.c -o /tmp/cadenza-l2-input-test
/tmp/cadenza-l2-input-test
```

本任务明确禁止刷写，因此这里没有 `flash` 命令。

### 已完成的静态验证（2026-07-22）

- ESP-IDF checkout：`v5.5`，commit
  `8c750b088c7cd857d079c0eeb495da199b359461`，目标 `esp32s3`。
- 从新的 `/tmp/cadenza-l2-input-build` 构建目录完成配置、bootloader、链接和镜像生成；
  应用镜像 `183,136 bytes`（`0x2cb60`），默认 1 MiB 应用分区剩余 83%。
- 应用镜像 SHA-256：
  `b5729794e867276939669c053dbcf58ba694661c1e253343412a692f70795b6a`。
- host C 测试以 `-Wall -Wextra -Werror` 通过，覆盖正反 Gray 序列、接点往返抖动、
  `cpd=2/4`、反向配置、非法跃迁后重新同步，以及按钮短按、长按只报告一次、长按
  释放不误报 `SHORT`。
- ISR 写入的 A/B 边沿及队列溢出统计使用 FreeRTOS ISR-safe `portMUX`；任务侧
  `status`/`reset` 通过同一临界区读取或清零，不依赖 `volatile` 掩盖跨上下文竞态。
- 本机原 ESP-IDF Python 环境受 macOS 动态库签名策略影响会卡住，因此本次用
  `/tmp` 中基于系统 Python 3.9 的隔离 venv 安装同一 ESP-IDF constraints 后构建；
  没有修改系统 Python，也没有把 venv 或构建产物写入工程。
- **没有执行 flash、esptool 写入或串口硬件测试。**

## 上板验证步骤（以后执行，不在本次构建中执行）

1. 全部断电，用通断档确认 EC12 A/C/B 和中心按键脚；单独给开发板供电测量 3V3，
   再次断电后照表接线。
2. 不按任何输入，上电；确认串口 raw/stable 都为 `0`。这里的 `0` 表示“未按下”。
3. A、B、Menu 各短按 20 次，再各长按超过 800 ms 5 次。预期每次只有一个
   `PRESS`/`RELEASE`，短按恰有一个 `SHORT`，长按恰有一个 `LONG` 且没有 `SHORT`。
4. 缓慢向一个方向转 20 个机械卡点，尝试 `cpd 1`、`cpd 2`、`cpd 4`，找出统计恰好
   变化 20 的值并记录；反方向重复。再用 `reverse 1` 选择符合产品交互的符号。
5. 两个方向各快速转至少 500 个卡点。`queue_overflow` 必须为 0；若 `invalid` 持续增长
   或卡点数可重复丢失，保存日志并检查 A/B 波形、RC 数值和任务调度，不能直接冻结。
6. A、B、Menu 各按 200 次，确认没有双触发；记录原始边沿数，以便区分机械抖动与
   软件事件错误。

## 当前证据边界

构建通过和 host test 只证明 ESP-IDF API、Gray 表、非法跃迁处理、可配置方向/每格计数
及按钮时间逻辑可编译并满足合成用例。它们不能证明真实 EC12 pinout、方向、每卡点脉冲、
最短脉宽、RC 是否合适，也不能代替 500 卡点与 200 次按键的实物记录。
