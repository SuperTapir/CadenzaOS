## Context

当前固件把应用生命周期、输入归一化和黑白绘图放在同一工程中，但核心头文件包含 `Arduino.h`，runtime/App 直接使用 `Serial`，输入同时承担 GPIO 采样与手势识别，画布的像素、字体和缓冲则实际由 `TFT_eSprite` 控制。结果是桌面端即使重新实现 `MonoCanvas`，也只能复用调用形式，不能证明像素、字体、输入时序或动画行为一致。

本 change 横跨核心 API、固件适配、桌面宿主、图形、动画、调试和测试。约束包括：ESP32-S3 资源有限；最终目标分辨率为 400×240 1-bit；当前 T-Embed 为 320×170；应用必须复用同一份源码；开发主机首先支持 macOS；P8 真机测量不在本 change 内执行。

调研结论：

- LVGL 将显示、输入和动画作为独立模块，timeline 支持 start offset、seek、reverse、repeat；借鉴契约，不引入完整 widget/runtime。[LVGL Animation](https://docs.lvgl.io/master/main-modules/animation.html)
- U8g2 同时支持 caller-owned full-buffer、基础图元、裁剪与字体；400×240 spike 约链接 6.5 KB code + 12 KB buffer、无 heap，且其 horizontal/MSB layout 与本项目契约一致，因此采用受控子集。[U8g2 Reference](https://github.com/olikraus/u8g2/wiki/u8g2reference)
- microui 以固定内存和渲染命令边界保持小型可移植；借鉴固定容量和显式 overflow，不引入其 immediate-mode widget 系统。[microui](https://github.com/rxi/microui)
- Tweeny 提供 typed tween、seek、正反 step 和 easing，但其通用模板 API 与正在进行的重写会扩大固件表面积；借鉴语义，自行实现稳定的小契约。[Tweeny](https://github.com/mobius3/tweeny)
- Noble 将场景转场建模为可选择的策略和属性；借鉴 Transition 对象，不引入完整游戏引擎。[Noble Engine](https://noblerobot.github.io/NobleEngine/modules/Noble.html)
- Taxman 用 C 平台适配器、离屏渲染、dither、atlas、transform animation 和可选 fixed-point 支撑 400×240 Sharp；证明这组边界适用于小型嵌入式 1-bit runtime。[Taxman Engine](https://github.com/McDevon/taxman-engine)
- SDL2 已进入维护/弃用状态；新桌面宿主使用 SDL3。SDL3 原生支持 logical presentation 与 integer scale，并有官方 CMake target。[SDL3 CMake](https://wiki.libsdl.org/SDL3/README-cmake)

## Goals / Non-Goals

**Goals:**

- 建立唯一像素真相、可移植输入语义和可控 simulation time。
- 让 Launcher、Clock、Motion、Settings 在固件、headless 和 SDL3 中运行相同核心源码。
- 提供适合 1-bit 表现的完整但克制的动画、转场和图形工具箱。
- 用 TDD、快照和桌面 E2E 证明 P1–P7 行为，并让失败可定位。
- 运行期核心不依赖堆分配，容量耗尽可观察且不会越界写入。

**Non-Goals:**

- 通用游戏引擎、ECS、物理、碰撞、Tilemap、关卡编辑器或脚本运行时。
- Sharp Memory LCD 真机驱动、声音、持久化、新业务应用。
- P8 的编码器、撕裂、真实 FPS、功耗或模拟器/TFT 实拍像素对比。
- 复制 Playdate SDK、商业游戏代码、受限字体、纹理或视觉素材。

## Decisions

### 1. 构建与源码边界

采用 C++17。共享源码放入 `lib/cadenza_core`；Arduino/TFT 适配放入 `lib/cadenza_t_embed`；固件组合根保留在 `src/main.cpp`；桌面宿主位于 `desktop/`；测试位于 `tests/`。PlatformIO 构建固件，顶层 CMake 构建 core、测试和 SDL3 桌面程序。

桌面采用系统 SDL3 CMake package；不自动联网 FetchContent。小型 header-only 测试/PNG/GIF 依赖以固定版本 vendoring，并在 `THIRD_PARTY_NOTICES.md` 记录来源、版本和许可证。

替代方案：PlatformIO native target 能少一个构建系统，但宿主链接、CTest、IDE 和跨平台 SDL 支持较弱；只用 CMake 管理 ESP32 会推翻当前稳定 PlatformIO 流程。双构建系统的成本由“共享 core target 清单检查”和 CI/本地双构建承担。

### 2. 唯一的 1-bit 像素真相

`MonoFramebuffer` 使用 row-major、MSB-first、`1 = black`、stride=`(width+7)/8`，支持 320×170 与 400×240，最大有效存储 12,000 bytes。`MonoCanvas` 在 caller-owned buffer 上包装受控 U8g2 raster/font 子集，并实现 Cadenza 专属 sprite/dither/diagnostic 操作；TFT、SDL、PNG、GIF 和测试只读取 framebuffer，不拥有另一套绘制语义。

所有图元使用整数坐标、左上原点和半开裁剪矩形。线段规定端点包含；矩形宽高小于等于零为空操作；wrapper 在调用 U8g2 前完成契约归一化和诊断，所有越界写入被裁剪，debug sink 同时记录 invalid/fully-clipped primitive。host 与固件链接同一 raster 代码，避免后端圆、线和字体算法差异。

依赖只纳入经过尺寸、内存、许可和像素测试的 U8g2 source/font；App 不接触 `u8g2_t`、设备 setup 或发送接口。这样复用成熟 raster/font，又不会把显示驱动矩阵、整套字体或第二个 Runtime 变成产品表面积。完整采用 LVGL 的对照 spike 约链接 461 KB code 和 78.6 KB data/BSS，并引入 object tree、动态 timeline 与独立内存池，因此不采用。[Adoption Decision](../../../docs/engine-adoption-decision.md)

### 3. 字体、bitmap 与资源格式

首个内置字体从 U8g2 字体中选择并逐项确认 provenance，`MonoCanvas` 暴露稳定的 measurement/alignment/baseline 契约；字体缩放只允许整数 nearest-neighbor。后续 bitmap/sprite 资源由离线工具转换成简单的只读 C/C++ 描述符，不在固件运行期解析 PNG/JSON。

`BitmapView` 可引用 packed 1-bit 数据；sprite 支持 source rect、clip、translate、flip X/Y 和三种合成模式：copy、set-black、xor。`SpriteAtlas` 是 bitmap 加固定容量 frame metadata。Pattern/Dither 使用可重复的 8×8 threshold/pattern 表；phase 是显式参数，确保动画可重放。反色区域使用 XOR。

### 4. 输入拆分为采样、归约和帧快照

平台后端产生带单调时间戳的 `RawInputEvent`（turn delta、button down/up）；`InputReducer` 是纯核心状态机，统一产生 pressed、released、clicked、longPressed、heldMs；`takeFrame()` 只清除瞬时字段。T-Embed GPIO 与 SDL 键鼠都不得自行判断短按或长按。

编码器 turn 累积使用带饱和的 `int16_t`，避免当前 `int8_t` 在高速旋转时回绕。长按阈值与 debounce 为显式配置。重复 keydown 不产生重复 press；滚轮和方向键都映射为 turn delta；Space/Enter 映射按钮。

### 5. 时间、日志和诊断服务

平台层拥有 wall/monotonic time，`SimulationClock` 产生独立 simulation delta。App 和动画只能消费传入的 `dt`；InputReducer 使用原始事件时间。暂停时 simulation time 停止，但宿主输入与调试控制继续工作。固定步长默认 1/60 s；变速仅缩放 simulation delta。

核心日志使用可选 `LogSink`/结构化 `DiagnosticEvent`，默认 no-op；固件适配到 Serial，桌面适配到 stderr/HUD。核心不得调用 Arduino/SDL 日志 API。

### 6. Runtime 与转场

`AppRuntime` 继续拥有静态 App 注册表与系统级长按返回。打开应用时，runtime 捕获 outgoing framebuffer，切换生命周期并渲染 incoming framebuffer；Transition 以两个只读 buffer、输出 buffer 和归一化 progress 工作。转场期间应用输入被冻结，但 debug/system controls 可用。

Transition 是非 owning 策略对象。基础实现：Dip/Fade（以 pattern coverage 表达 1-bit 灰阶）、horizontal wipe、diagonal wipe、iris、venetian blinds、checker/dither dissolve。统一规定 progress=0 精确等于 source、progress=1 精确等于 destination，中点切换不能泄漏未初始化像素。

成本是至少需要 source、destination、output 三张最大 buffer（约 36 KB）；收益是所有转场、截图和测试有相同像素语义。若板端内存测量证明不可接受，后续可把 output 与显示 buffer 复用，但本 change 不退回后端专属转场。

### 7. SDL3 桌面宿主

SDL3 宿主把 framebuffer 扩展为 RGBA streaming texture，使用 nearest scale 和 logical integer presentation；可选择 320×170 或 400×240，并提供 1×、2×、3×、4×窗口尺寸。窗口循环、键鼠、文件输出、HUD 和设备外框只存在于 desktop 层。

无窗口 `HeadlessHost` 先于 SDL 存在，用于固定 dt、脚本输入、快照和 CI。桌面 E2E 使用 SDL dummy/offscreen driver 或 headless host 验证完整 Launcher/App 路径，不把人工看窗口当唯一证据。

### 8. 模拟器调试与录制

调试状态由 `SimulationController` 管理：pause/resume、single-step、0.25×/0.5×/1×/2×、fixed/real-step。HUD 显示 FPS、frame time、当前 App、输入状态、核心固定容量使用量和平台可提供的 heap estimate。

PNG/GIF/帧序列 writer 在桌面层把同一 framebuffer 转成文件；截图命名包含 profile、frame index 和 simulation time。录制按 simulation frame 采样，暂停不重复写帧。GIF 是便利输出，PNG 序列是无损/可测试的权威录制格式。

### 9. 动画核心

`Easing` 是无分配函数指针，输入输出均为 normalized float；默认 clamp 输入到 [0,1]，端点必须精确返回 0/1。实现 Linear、In/Out/InOut Quad、InOut Cubic、Out Expo、Out Back、Out Bounce、Out Elastic。

`Tween<T>` 支持 arithmetic scalar、delay、repeat count、repeat delay、yoyo/reverse、completion callback、progress query/seek。组合使用非 owning `Playable*`：`Sequence` 串行分配局部时间，`Parallel` 同步分配局部时间，`Timeline` 以绝对 start offset 放置条目；容器固定容量，添加失败返回状态。调用者负责被组合对象生命周期。

动画 seek 是权威操作：相同 progress 必须得到相同值且不依赖此前 update 历史。completion callback 只在播放方向跨越完成边界时触发一次，纯 seek 默认不触发副作用。Spring 是独立的状态动画，使用 semi-implicit Euler 和有上限的固定子步；它不伪装成可任意 seek 的 Tween。

替代方案：Tweeny 的 typed tween/seek 成熟，但最小 spike 构造即发生 3 次分配/400 bytes，注册 callback 再发生 2 次/72 bytes，且 delay/repeat/composition/Spring 仍需外包；因此不直接采用。`std::function`、动态数组和 owning polymorphism 会隐藏分配与失败。全 fixed-point 可提高跨架构确定性，但复杂 easing/elastic 和 API 成本过高。本阶段使用 float、精确端点和容差数值测试；图元栅格化保持整数确定性。

### 10. 表现层

选中反馈使用 Tween/Spring 的 overshoot/pulse；camera punch 是短促位移/缩放包络，camera shake 使用带显式 seed 的确定性 PRNG，不调用全局随机数。粒子池固定容量，满时按策略拒绝新粒子或替换最老粒子并计数；不得分配。

序列帧动画引用 atlas frame range，支持 loop/once/ping-pong；状态机使用固定容量 state/transition 表和显式触发。Animation Gallery 是正常注册 App，分页面展示 easing、composition、spring、transitions、camera、particles、sprites/dither，并允许旋钮 scrub 当前演示的 progress。

视觉原则是高对比、清晰节奏、短反馈、可中断、少量 overshoot；默认动画时长与幅度由 gallery 实机前的桌面审美回归确定。提供 reduced-motion/quiet 参数，使 Settings 能压低 shake、elastic 和粒子密度。

### 11. TDD 与完成证据

每个实现任务遵守 red-green-refactor：先提交能说明缺失行为的自动测试；再实现最小完整能力；最后跑相关测试、全套测试、格式/`git diff --check` 和真实桌面流程。测试按以下层级组织：

1. 单元：input、framebuffer、raster、easing、Tween、组合、Spring、particle/state machine。
2. 集成：App lifecycle、系统返回、transition source/destination、simulation controls。
3. 快照：两种分辨率的关键 App/Gallery 帧；快照包含尺寸和 framebuffer hash，更新必须显式审核。
4. E2E：从 Launcher 经键盘/滚轮进入三个 App 与 Gallery、长按返回、暂停/单帧、截图/录制。
5. 构建回归：host tests、desktop target、PlatformIO T-Embed target。

对浮点动画使用语义断言和 epsilon，不以整块内存 hash 证明跨 CPU 浮点一致；对软件栅格器和固定输入/时间的最终 framebuffer 使用精确 hash。

## Risks / Trade-offs

- [一次 change 覆盖 P0–P7，影响面大] → tasks 按可独立验证的里程碑排序并做 WIP commits；任何阶段保持 host tests 与固件构建可恢复。
- [CMake 与 PlatformIO 源文件清单漂移] → core 是单一目录 target，并增加构建测试证明两端包含相同核心模块。
- [三张 400×240 buffer 增加约 36 KB 静态内存] → 编译期 size assertion、容量诊断；P8 再依据板端测量优化复用。
- [U8g2 wrapper 或自研 animation 产生维护成本] → 基础 raster/font 复用经 spike 的 U8g2 子集；Cadenza API 严格限制在 P1–P7，并以像素/数值测试锁定；不扩展成 widget 或游戏引擎。
- [固定容量可能丢动画、粒子或 atlas 条目] → 所有 add/spawn 返回显式结果，HUD 暴露 overflow 计数，测试覆盖满容量。
- [GIF writer 质量和文件大小有限] → PNG sequence 是权威输出，GIF 只作快速分享。
- [视觉“舒适”无法完全由单测证明] → Gallery 建立统一对比面，关键时长/振幅集中配置，并保留 reduced-motion；P8 才做真机触感验证。
- [桌面已装 SDL2、但缺 CMake/SDL3] → 用 Homebrew 安装 CMake/SDL3 并记录 bootstrap；不因此降级到已弃用 SDL2。
- [第三方素材许可证风险] → 只 vendoring 代码型依赖与许可证；字体/纹理由公共领域、明确许可或自行创作来源组成，记录 provenance。

## Migration Plan

1. 在 feature branch 创建构建/test skeleton，证明当前 App 核心因 Arduino/TFT 耦合无法 host 编译。
2. 引入新 core 类型、input reducer、framebuffer/raster，并逐个迁移 App；旧路径在新固件构建通过前保留。
3. 将 TFT 后端改为只 present framebuffer，删除旧 TFT-owned canvas。
4. 增加 headless runner 和 SDL3 host，完成四个现有 App 的双分辨率闭环。
5. 在稳定核心上逐层加入 debugger、animation core、presentation、graphics 与 Gallery，每层先测试。
6. 跑全套 host/desktop/PlatformIO 验证后删除兼容代码，更新架构文档和第三方声明。

若某里程碑失败，可回退该里程碑的 WIP commit；不混合重写尚未迁移的后端和下一层动画功能。

## Open Questions

- 无阻塞性产品问题。第一阶段正式支持 macOS；Windows/Linux 保持 SDL3/CMake 可移植结构，但不宣称已经 E2E 验证。
- Gallery 的最终节奏和默认 reduced-motion 幅度需在桌面视觉 QA 后确定；真机触感属于 P8。
