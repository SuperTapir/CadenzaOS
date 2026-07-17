# Cadenza

**Cadenza** 是由 Tapir 创作的个人互动设备与创作平台；它的系统称为
**Cadenza OS**。第一版 Cadenza OS 面向原版 LILYGO T-Embed（非 CC1101 版本）
构建，以它作为交互与软件架构的原型硬件。

项目的长期目标、设计动机和硬件路线记录在 [`docs/project-vision.md`](docs/project-vision.md)。
当前平台的职责边界和后续路线记录在 [`docs/platform-architecture.md`](docs/platform-architecture.md)。

当前功能：

- 320×170、内存中 1-bit 双缓冲的黑白 Launcher；
- 统一的应用生命周期与应用切换转场；
- 旋转选择、短按进入、长按返回 Launcher 的输入模型；
- 可实际进入的 Clock、Motion Study、Settings 三个内置应用；
- 60 FPS 动画循环与纯图元动态视觉；
- USB 串口输出启动、Flash、PSRAM 和输入事件；
- Cursor 中的编译、烧录和串口任务。

## 在 Cursor 中打开

使用“文件 → 打开文件夹”，选择本项目目录。然后选择“终端 → 运行任务”：

- `Cadenza OS: 编译（T-Embed）`
- `Cadenza OS: 烧录（T-Embed）`
- `Cadenza OS: 串口日志（T-Embed）`

也可以在项目目录的终端中运行：

```bash
../.platformio-env/bin/pio run
../.platformio-env/bin/pio run --target upload
../.platformio-env/bin/pio device monitor --baud 115200
```

## 重要

当前引脚来自 LILYGO 官方原版 T-Embed 配置。不要烧录到 T-Embed CC1101；收到设备后应先根据包装和主板丝印确认版本。
