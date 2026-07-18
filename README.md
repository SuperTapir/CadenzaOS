# Cadenza

**Cadenza** 是由 Tapir 创作的个人互动设备与创作平台；它的系统称为
**Cadenza OS**。第一版 Cadenza OS 面向原版 LILYGO T-Embed（非 CC1101 版本）
构建，以它作为交互与软件架构的原型硬件。

项目的长期目标、设计动机和硬件路线记录在 [`docs/project-vision.md`](docs/project-vision.md)。
当前平台的职责边界和后续路线记录在 [`docs/platform-architecture.md`](docs/platform-architecture.md)。
桌面构建与测试环境记录在 [`docs/development.md`](docs/development.md)。

当前功能：

- 320×170 T-Embed 与 400×240 Sharp profile 共用的 1-bit framebuffer；
- Launcher、Clock、Motion、Settings、Animation Gallery 共用同一套 App/Runtime；
- SDL3 桌面模拟器，支持 1×–4× 整数缩放、键鼠输入和设备外框；
- 旋转选择、短按进入、长按返回 Launcher 的输入模型；
- allocation-free Tween、Timeline、Spring、转场、camera effects、粒子与
  atlas 序列帧状态机；
- 暂停、单帧、时间倍率、PNG 截图、PNG/GIF 录制和调试 HUD；
- 不接硬件即可运行的生命周期、输入、像素快照、动画与桌面 E2E 测试。

项目定位是“1-bit 交互与动画运行时”，不是完整游戏引擎。物理、碰撞、
Tilemap、ECS 和关卡系统当前明确不在范围内。引擎/库复用边界记录在
[`docs/engine-adoption-decision.md`](docs/engine-adoption-decision.md)。

## 桌面模拟器

```bash
brew install cmake sdl3
cmake --preset host-debug
cmake --build build/host --parallel
./build/host/cadenza_desktop --profile t-embed --scale 2 --overlay
```

开发时可使用零额外依赖的模拟器脚本。`dev` 会监听 C/C++ 与 CMake 文件，
增量编译成功后自动重启模拟器；编译失败时保留当前进程并继续等待修改：

```bash
./tools/simulator.py dev --profile t-embed --scale 2 --overlay
./tools/simulator.py build
./tools/simulator.py run --profile sharp --device-frame
./tools/simulator.py package
```

`package` 在 `dist/` 下生成包含 SDL3 的 macOS `.app` 和 zip。它适合本机运行
和同架构 Mac 的开发构建，不是经过 Developer ID 签名、公证的正式发行包。

旋钮可用鼠标滚轮或左右方向键；空格/回车模拟按键。F1 切换 HUD，F2
暂停，F3 单帧，F4 切换 fixed/real step，F5 循环时间倍率，F6 截图，F7
开始/停止录制。完整构建、测试和控制说明见
[`docs/development.md`](docs/development.md)。

```bash
tools/check.sh host
tools/check.sh desktop
tools/check.sh firmware
```

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

P0–P7 的主机、桌面和固件编译门禁已经建立；编码器手感、TFT 撕裂、真实
FPS/最慢帧与模拟器/真机像素实拍对比属于 P8，必须在实体硬件上完成，不能
用桌面结果冒充。当前证据见 [`docs/verification.md`](docs/verification.md)。
