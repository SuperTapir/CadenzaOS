## Why

Cadenza OS 已能在 T-Embed 上展示 Launcher 和三个应用，但核心仍依赖 Arduino、TFT_eSPI 的私有画布与字体，因而无法在桌面可靠复现、自动测试或持续打磨动画。现在需要把它收敛为一个可移植、确定性、固定容量的 1-bit 交互与动画运行时，以桌面模拟器和 TDD 建立后续视觉实验的快速闭环。

## What Changes

- 明确当前阶段定位为 Cadenza OS 的 `1-bit 交互与动画 runtime`；物理、碰撞、Tilemap、ECS 和关卡系统为非目标，长期仍可承载小游戏、工具和互动实验。
- **BREAKING** 将应用/runtime/画布/输入核心从 Arduino 和 TFT_eSPI 类型中解耦，并以标准 C++17 构建同一份核心源码。
- **BREAKING** 用规范化、行优先、packed 1-bit framebuffer 和 U8g2-backed 软件栅格层替代由 TFT_eSprite 持有像素真相的设计；Cadenza wrapper 继续拥有契约，TFT 与桌面仅负责提交或展示 framebuffer。
- 将原始输入后端、短按/长按状态机和每帧 `InputFrame` 分层；T-Embed 与桌面共享相同手势语义。
- 新增 SDL3 桌面模拟器、320×170/400×240 配置、1×–4×整数缩放、键鼠旋钮映射与统一 App/Runtime 运行链路。
- 新增暂停、单帧、变速、固定步长、诊断叠层、PNG 截图、GIF/帧序列录制、越界检查和可选设备外框。
- 新增固定容量的 easing、Tween、Sequence、Parallel、Timeline、Spring，以及可寻址、可反向、可重复的动画控制。
- 新增可组合转场、反馈动画、camera punch/shake、固定容量粒子、序列帧/状态机和 Animation Gallery。
- 补齐 bitmap/sprite/atlas、裁剪、平移/翻转、像素字体、pattern/dither、动态相位、反色与离屏画布。
- 将测试前移为 P1–P6 的开发门槛：每项能力先建立失败用例，再实现；覆盖输入、生命周期、转场、动画数值、图元和两种分辨率的截图快照。
- P8 真机性能与像素回归不在本 change 内执行；本 change 只保留板端构建和未来真机验收所需的诊断接口。

## Capabilities

### New Capabilities

- `portable-runtime`: 标准 C++ runtime、平台服务边界、应用生命周期、时间、日志和统一输入语义。
- `mono-graphics`: 规范 framebuffer、确定性软件绘制、字体、sprite、atlas、裁剪、变换、抖动和离屏画布。
- `desktop-simulator`: SDL3 桌面运行器、显示配置、整数缩放、键鼠交互和与固件共用的组合根。
- `simulation-debugging`: 确定性时间控制、诊断叠层、截图/录制、边界检查和设备预览。
- `animation-core`: 固定容量 easing、Tween、组合动画、Timeline、seek/reverse/repeat 与 Spring。
- `animation-presentation`: 转场、选中反馈、camera effects、粒子、序列帧状态机和 Animation Gallery。
- `runtime-verification`: TDD 门槛、单元/集成/快照测试、headless 测试链路和板端编译回归。

### Modified Capabilities

无。当前仓库尚无已发布的 OpenSpec capability。

## Impact

- 主要影响 `include/`、`src/`、`platformio.ini`，并新增标准 C++ 核心、桌面宿主、测试、资源和 CMake 构建结构。
- 固件继续使用 Arduino 与 TFT_eSPI，但依赖被限制在 T-Embed 后端和固件组合根；应用与 runtime 不再可直接调用 `Serial`、GPIO、`millis()` 或 `micros()`。
- 桌面端新增 SDL3；测试采用轻量 C++ 测试框架；PNG/GIF 写出采用许可证兼容的桌面侧小型库并记录第三方声明。
- 现有 `MonoCanvas`、`InputController` 和 `TftMonoCanvas` API 将发生不兼容调整；Launcher、Clock、Motion、Settings 会迁移到新契约。
- 采用固定容量和显式失败的资源策略，以换取 ESP32 上可预测的内存与桌面/固件一致的行为边界。
