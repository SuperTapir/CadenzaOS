# Cadenza OS

[English](README.md) · **简体中文**

<p align="center">
  <img src="assets/about/source/about_logo.png" alt="Cadenza OS — A Space to Improvise." width="700">
</p>

**Cadenza** 是由 Tapir 创作的个人互动设备与创作平台；它的系统称为
**Cadenza OS**。当前版本以原版 LILYGO T-Embed（非 CC1101 版本）为原型硬件，
探索资源受限设备上的 1-bit App/Runtime、系统交互与动画语言。

https://github.com/user-attachments/assets/4cbb17ba-b52b-44dc-b557-88dd9fdf3d63

上方视频展示带声音的完整应用流程：Launcher、Timer、后台 Timer 指示器、
System Menu、Motion、Settings 与 Animation Gallery，以及 T-Embed 320×170 和
Sharp Memory LCD 400×240 两种显示 profile 下的表现。

> **原型状态：**桌面、headless 与固件编译门禁已经建立；编码器手感、屏幕表现、
> 真实帧率和扬声器听感仍需在原版 T-Embed 真机上验证。

> **非商业爱好者项目：**Cadenza 是出于设计与创造乐趣而制作的个人项目，
> 没有将其商业化、销售或经营为业务的打算。

## 动机

“Cadenza”原指乐曲中留给演奏者自由发挥的华彩段。这个名字对应项目的核心：
在统一而克制的设备中，为小游戏、工具、动画和交互实验留下个人表达的空间。

Cadenza 源于我对 Playdate 设计与艺术风格的喜爱。真正吸引我的不只是“掌机”，
而是它作为一件数字物件所呈现的整体气质：反射式黑白屏、直接的物理输入、
富有表现力的动画，以及数量不多却有明确取舍的功能。Cadenza 中许多 Launcher
行为、交互节奏、动画和视觉细节，都在有意识地研究并尝试复刻 Playdate 的实现
与体验。重新实现这些想法，是为了理解其背后的设计技艺，并把学到的东西带入
一个个人平台，而不是为了运行或替代 Playdate 软件。

T-Embed 是第一阶段的软件和交互原型。长期硬件方向以 400×240 Sharp
`LS027B7DH01` Memory LCD 和专用掌上设备为核心。软件结构需要让原型证明平台，
而不是让临时硬件定义最终产品。

## 核心能力

- 320×170 T-Embed 与 400×240 Sharp profile 共用同一套 1-bit framebuffer、
  App/Runtime 和布局规则；
- Launcher、Timer、Motion、Settings、Animation Gallery 五个内置 App，支持
  旋转选择、短按进入、长按系统菜单与显式返回 Launcher；
- 后台 Timer、到期提示、系统菜单、会话音量、Reduced Motion 与 Launcher
  横竖方向等系统级服务；
- 18 项语义音效，桌面 SDL callback 与 T-Embed I²S task 共用 44.1 kHz
  四声部合成核心；
- allocation-free Tween、Timeline、Spring、转场、camera effects、粒子与
  atlas 序列帧状态机；
- SDL3 桌面模拟器、headless deterministic host、PNG/GIF 录制、像素快照与
  生命周期、输入、动画、音频和系统服务测试。

项目定位是“1-bit 交互与动画运行时”，不是完整游戏引擎。物理、碰撞、
Tilemap、ECS 和关卡系统当前明确不在范围内。

## macOS 快速开始

```bash
brew install cmake sdl3
./tools/simulator.py run --profile t-embed --scale 2 --overlay
```

开发时可以启用源码监听：

```bash
./tools/simulator.py dev --profile t-embed --scale 2 --overlay
```

常用显示选项包括 `--profile t-embed|sharp`、`--scale 1..4`、
`--palette reflective|pure`、`--overlay` 与 `--device-frame`。默认的
`reflective` 色板近似反射式 Memory LCD；`pure` 用于检查纯黑白输出。两者只改变
桌面呈现，不改变 framebuffer、截图、快照 hash 或固件输出。

## 桌面操作

| 输入 | 行为 |
| --- | --- |
| 鼠标滚轮或 `Left` / `Right` | 旋转编码器 |
| 短按 `Space` / `Enter` | 按钮点击 |
| 长按 `Space` / `Enter` | 打开 System Menu |
| `F1` | 切换调试 HUD |
| `F2` / `F3` | 暂停或恢复 / 暂停时单步 |
| `F4` / `F5` | 切换 fixed/real step / 循环时间倍率 |
| `F6` / `F7` | PNG 截图 / 开始或停止 PNG+GIF 录制 |

完整控制、Timer 行为、显示参数与 Launcher Cover 工作流见
[`docs/development.md`](docs/development.md)。

## 构建与验证

```bash
tools/check.sh host      # host 构建与完整测试
tools/check.sh desktop   # SDL3 构建与启动 smoke
tools/check.sh firmware  # PlatformIO T-Embed 编译
tools/check.sh all       # 完整验证矩阵
```

固件也可以直接通过仓库旁的 PlatformIO 环境构建、烧录和查看串口日志：

```bash
../.platformio-env/bin/pio run
../.platformio-env/bin/pio run --target upload
../.platformio-env/bin/pio device monitor --baud 115200
```

桌面模拟器可以打包为包含 SDL3 的 ad-hoc-signed macOS `.app` 和 zip：

```bash
./tools/simulator.py package
```

该产物适合本机和同架构 Mac 的开发验证，不是经过 Developer ID 签名与公证的
正式发行包。

## 硬件安全

当前引脚来自 LILYGO 官方原版 T-Embed 配置。**不要烧录到 T-Embed CC1101**；
收到设备后应先根据包装和主板丝印确认版本。

## 开发方式披露

本仓库的大部分源代码由 **OpenAI Codex** 在 Tapir 的指导下生成和迭代。
Tapir 负责产品愿景、需求、架构决策、审查标准和验收条件；Codex 承担了大量
实现、重构、文档和测试编写工作。

项目不会把 AI 生成代码视为天然可信。被接受的改动会尽可能通过构建、测试、
像素快照、调研记录和真机门禁检查。Tapir 仍是项目的创作者与维护者，并对最终
进入仓库的内容负责。

## 鸣谢

- 特别感谢 [Panic](https://panic.com/) 与 [Playdate](https://play.date/) 团队。
  没有我对 Playdate 工业设计、艺术风格、交互设计和动画技艺的喜爱与敬意，
  Cadenza 就不会存在。本项目会坦率地研究并尝试复刻其中许多交互模式与体验
  品质。Cadenza 仍是独立项目，不兼容 Playdate 软件，也未获得 Panic 背书。
  仓库中的 Playdate Roobert 字体子集按 CC BY 4.0 使用，详情见
  [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)。
- [LILYGO](https://www.lilygo.cc/) 创作了第一阶段原型使用的 T-Embed 硬件，并
  发布了平台适配所参考的资料。
- SDL3、U8g2、doctest、stb_image_write 与 gif-h 支撑了可移植构建、绘制、测试
  和桌面工具。准确版本、复用边界与许可证见
  [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md)。

## 延伸文档

- [`docs/project-vision.md`](docs/project-vision.md)：长期目标、设计动机与硬件路线；
- [`docs/platform-architecture.md`](docs/platform-architecture.md)：平台职责边界与路线；
- [`docs/development.md`](docs/development.md)：完整开发、测试和模拟器说明；
- [`docs/verification.md`](docs/verification.md)：当前证据、门禁与真机验证脚本；
- [`docs/engine-adoption-decision.md`](docs/engine-adoption-decision.md)：引擎与库的
  复用边界；
- [`docs/audio-reference-research.md`](docs/audio-reference-research.md)：音频调研与
  采用决策；
- [`docs/audio-asset-contract.md`](docs/audio-asset-contract.md)：未来 WAV 资产的
  交付与验收规则。
