# Cadenza

**Cadenza** 是由 Tapir 创作的个人互动设备与创作平台；它的系统称为
**Cadenza OS**。当前版本以原版 LILYGO T-Embed（非 CC1101 版本）为原型硬件，
探索资源受限设备上的 1-bit App/Runtime、系统交互与动画体验。

https://github.com/user-attachments/assets/4cbb17ba-b52b-44dc-b557-88dd9fdf3d63

上方视频展示带声音的完整应用流程：Launcher、Timer、后台 Timer 指示器、
System Menu、Motion、Settings 与 Animation Gallery，以及 T-Embed 320×170 和
Sharp Memory LCD 400×240 两种显示 profile 下的表现。

> 当前处于原型阶段。桌面、headless 与固件编译门禁已经建立；编码器手感、
> 屏幕表现、真实帧率和扬声器听感仍需在原版 T-Embed 真机上验证。

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

开发时可以启用源码监听；增量编译成功后模拟器会自动重启，编译失败时保留
当前进程并等待下一次保存：

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

## 硬件安全与验证状态

当前引脚来自 LILYGO 官方原版 T-Embed 配置。**不要烧录到 T-Embed CC1101**；
收到设备后应先根据包装和主板丝印确认版本。

P0–P7 与音频的主机、桌面和固件编译门禁已经建立；编码器手感、TFT 撕裂、
真实 FPS/最慢帧、模拟器/真机像素实拍和扬声器听感仍必须在实体硬件上完成，
不能用桌面结果冒充。当前证据与真机脚本见
[`docs/verification.md`](docs/verification.md)。

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
