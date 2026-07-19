## Context

`LauncherApp::update()` 当前把整数 `selected_` 映射到浮点 `position_` 并逐帧逼近，但 `render()` 只读取 `selected_`，所以任何输入都会先瞬时替换中央标题。若简单让渲染读取现有 `position_`，末项到首项时又会从 `count - 1` 跨越整条列表到 `0`，仍不符合循环轨道的空间语义。

项目已经具备无堆分配的 Spring/Tween、统一 `MotionProfile`、受约束文本、App catalog、SettingsWrite capability、headless 双 framebuffer profile 和确定性固定步测试。Playdate 1.12.3 本地调研确认卡片应承担 App 身份，并应把选择、按下、加载和首屏视为连续体验；其官方 `CoreLibs/ui/gridview.lua` 默认以 250 ms `outCubic` 执行无 overshoot 的滚动。Launcher 本身未开放源码，因此这里只采用 Playdate 公共 UI 的快速响应、单调减速和精确停靠语义，不声称复制其私有实现。同时 1-bit 滚动需要避免纹理逐像素相位闪烁。当前系统设置为会话态，项目尚无通用用户 preference 持久化契约。

对 Playdate SDK 1.12.3 `Inside Playdate` 与本机 `Examples/Level 1-1/SystemAssets` 的源码级复核进一步确认：`card.png` 是 350×155 的内容场景，`card-highlighted/` 用内容变化表达选中，`card-pressed.png` 响应按下，400×240 `launchImages/` 以 20 FPS 衔接首屏。示例使用 BSD-0；本 change 只采用卡片作为 App 识别物与固定画布的思想，明确不采用状态化 Cover，所有 Cadenza Cover 均原创，不复制其像素、美术或品牌。

## Goals / Non-Goals

**Goals:**

- 以同一套轴向布局实现纵向和横向连续循环卡片轨道。
- 输入改变时立即更新逻辑选择，并让视觉位置从当前状态连续 retarget，不等待前一段动画完成。
- 保证末项/首项切换只移动一个卡片间距，移动中点击打开最新逻辑选择。
- 让 Normal 与 Reduced Motion 都保留方向和连续性，并都以无 overshoot 的单调 ease-out 精确停靠；Normal 使用 Playdate 公共 UI 的 250 ms 节奏，Reduced 缩短位移时间。
- 在双显示 profile 下显示当前和相邻卡片，动态名称始终受卡片可见内区约束。
- 让每个内置 App 以原创内容场景而非统一大字黑框建立可辨识 Cover，并为没有 Cover 的 App 提供确定 fallback。
- 把 Cover 收敛为静态、不可交互的 App 身份图；Launcher 只移动和裁剪整张图，不向 Cover 传递选中、按下或启动状态。
- 通过现有权限命令边界让 Settings 的方向设置同帧可见，并保持 headless/firmware 共享实现。

**Non-Goals:**

- 不引入运行时 PNG/PDI 文件加载、逐帧动画描述文件、运行时缩放或外部可安装 App 资源协议；首版只使用离线转换、编译进固件的 1-bit 静态 Cover。
- 不改变 Runtime 的 App 启动转场策略与生命周期。
- 不建立 NVS、文件或跨重启设置持久化子系统。
- 不重构全系统输入模型，也不复制 Playdate 的字体、图像或品牌资产。

## Decisions

### 1. 逻辑选择与视觉轨道位置分离

Launcher 保存无界的整数目标位置和连续浮点视觉位置。目录索引只在访问 App catalog 时通过正模映射：

```text
targetPosition: ... 3 → 4 → 5 ...
catalogIndex:       3 → 4 → 0      (count = 5)
visualPosition:     连续追赶 targetPosition
```

输入 `turn` 原样累加到目标位置，因此快速输入不会丢失选择步数；一个输入帧仍只产生一个现有的语义导航 cue。视觉稳定且绝对位置达到安全阈值后，目标和视觉位置一起重基到等价的小整数坐标，避免长期运行的整数溢出与 float 精度损失。

未采用“只保存环绕后的 selected index”：它无法区分 `4 → 0` 应向前一步还是向后四步。未采用排队播放每个 Tween：连续旋转会积累延迟，手感落后于输入。

### 2. 使用轴向参数化的固定容量绘制

渲染根据 `visualPosition` 附近的固定数量逻辑 slot 计算卡片中心：

```text
cardCenter = viewportCenter + (slot - visualPosition) * cardPitch
```

纵向只改变 y，横向只改变 x；卡片尺寸、pitch、viewport 和导航指示器由当前 framebuffer 比例确定。每帧最多检查固定数量 slot，不分配容器。slot 再通过正模映射到 catalog App。

轨道 viewport 位于轻量标题栏下方并延伸到屏幕底部，让纵向布局在 320×170 仍能露出上下 Cover。部分可见卡片的填充、边框和 Cover bounds 在提交给 `MonoCanvas` 前显式求交；相邻卡片可被有意裁剪，但不依赖 framebuffer 越界，也不产生误用诊断。相邻 Cover 本身已经同时表达顺序和方向，因此删除重复的前后名称与页码 footer。

未采用为每张卡片建立 framebuffer 再整体 blit：两个 profile 下会显著增加固定 RAM。Cover renderer 直接接收完整内容 bounds；Launcher 以显式 clip 控制相邻卡片的可见区域。350×155 与 280×124 的 1-bit packed bitmap 均从同一高分辨率 PNG 母图直接离线生成；禁止把已二值化的 350×155 PBM 再缩小，因为实测会使 Motion/Settings 细线碎裂并增加孤立连通块。Launcher 在横纵方向都围绕当前 profile 的完整图片设置 card bounds；renderer 直接 blit 对应尺寸，不裁切当前 Cover 内容、不运行时缩放、不分配临时 framebuffer，因而同时避免标题缺损、缩放伪影与额外 RAM。

### 3. 两种 Motion profile 都使用无 overshoot 的单调 ease-out

Normal profile 使用 250 ms `outCubic`，与 Playdate SDK 1.12.3 公共 `gridview` 的默认滚动曲线和时长一致：输入后快速响应，随后单调减速并精确停在目标，不发生越界回拉。Reduced Motion 使用相同的单调曲线但缩短到 160 ms，减少运动暴露时间而不瞬切。新输入或 profile 切换都从当前视觉位置重新起算，位置本身不跳变；到达时长后精确吸附目标，保证稳定快照。

未采用欠阻尼 Spring：实机/模拟器观感会在越过目标后回拉，造成用户可感知的“抖动”和不精准。未声称 Playdate Launcher 私有实现使用 `gridview`；这里锁定的是同版本 SDK 中可审计的官方公共 UI 默认值与其无 overshoot 语义。

未采用 Reduced Motion 直接瞬切：这会丢失方向和空间连续性，违反现有“核心导航反馈不能禁用”的契约。

### 4. Launcher 方向是受权限约束的会话设置

在 core 公共类型中增加 `LauncherOrientation { Vertical, Horizontal }`，并加入 `SystemSnapshot`。新增 `SetLauncherOrientation` command，与 Motion/Sound 一样要求 `SettingsWrite`，由 `SystemServiceHost` 校验枚举并在 commit 时更新 snapshot。Settings 增加一行循环切换，Launcher 每帧从 snapshot 消费方向。

未把方向保存在 `LauncherApp` 或 `SettingsApp` 私有字段：那会绕过 capability 边界，也无法让 headless、desktop 和 firmware 观察同一状态。未在本 change 中加入持久化，因为当前没有可复用的 preference store，顺带建立 NVS 契约会显著扩大迁移和故障恢复范围。

### 5. 输入、声音与打开语义保持即时

旋转时逻辑选择立即改变，选择诊断和 Navigate cue 仍基于实际目录变化。点击直接打开目标位置映射出的 App；Runtime 捕获 outgoing Launcher frame 时 Cover 像素与点击前完全一致，即使视觉轨道尚未停稳。按钮按下、长按系统菜单、被拒绝的重复 open 都不向 Cover 传递状态。空目录继续安全显示 `NO APPS`。方向切换不改变当前选择，不播放 Launcher 内额外导航 cue。

### 6. 1-bit 背景稳定在屏幕坐标

移除 Launcher 背景纹理随 `time_` 的逐像素漂移，并把装饰背景降为低干扰的屏幕坐标静态元素；系统 chrome 只保留单层细边、必要间距和轻量标题栏。Cover 内部可使用与 App 语义相关的固定图案，但不得用逐像素反相纹理制造选中动画。连续动画以稳定 30 FPS 为真机目标；host 固定步只证明确定性，不能替代真机拖影和帧时间检查。

### 7. Cover 是 App 的可选纯绘制契约，内置图使用离线 1-bit 资源

`App` 增加默认返回 `false` 的 const Cover renderer，只接收 `MonoCanvas` 和已解析 bounds。接口刻意没有 selection、button、launch、time 或 lifecycle 参数；`AppCatalogView` 提供窄的 `renderLauncherCover` 转发，不把可变 App 指针暴露给 Launcher。返回 `false` 时 Launcher 在同一 bounds 内绘制原创的通用 fallback 和受约束 App 名称。

Clock、Motion、Settings 与 Gallery 分别使用人工确认的原创插图。同一高分辨率源图分别以固定 LANCZOS fit 和固定阈值直接转换到 350×155、280×124，再生成 packed 1-bit header；生成物记录来源与转换命令并接受母图→PBM 与 PBM→header 两层可复现性测试。renderer 只根据 bounds 选择 profile 对应图片并执行等尺寸 blit，必须无分配、确定、不得调用或观察 App lifecycle/update 状态，也不得越过传入 bounds。选中、按下、长按、启动以及 App 自身状态变化前后的同 bounds 渲染必须逐像素一致；轨道动画只能改变整张 Cover 的位置与可见裁剪区域。

未采用 Launcher 按 `AppId` switch 绘制：这会把 App 身份重新耦合进系统并破坏可扩展注册。未采用在 Cover 接口中预留 Highlighted/Pressed/Launching：预留状态会鼓励输入时变异封面，并已导致 Clock 时间跳变、Motion 圆形变黑等实际缺陷；未来启动动画应独立调研资源、内存和转场契约。未采用运行时图片加载或缩放：当前四张图尺寸固定且随固件发布，离线打包的可审计成本更小；外部 App 资源、缩放算法和许可证边界仍应另立 change 调研。

### 8. Desktop 反射屏色板只属于呈现层

Playdate 1.12.3 设计规范明确说明 Simulator 可把纯黑白映射为接近良好照明下
Memory LCD 的灰色。对本地参考图的两色样本复核得到 ink `#322F28`、paper
`#B1AEA7`。SDL 默认使用该 `reflective` 色板，同时提供 `--palette pure` 映射到
`#000000`/`#FFFFFF`，用于检查原始对比。

色板转换只发生在 1-bit framebuffer 上传 SDL texture 时；PNG/GIF、framebuffer
hash、headless 输出和 firmware presenter 继续消费原始 bit。未在 framebuffer 中
引入灰阶，因为那会破坏跨平台确定性和 1-bit 产品契约。该色板只是观看近似，不能
代替真实 Memory LCD 的环境光、观看角度和无背光验收。

macOS 窗口同时请求 SDL `HIGH_PIXEL_DENSITY` backing，并在启动日志报告逻辑窗口、
实际 backing 像素和 density。Retina 上实测 640×340 逻辑窗口对应 1280×680 backing、
2.00x density；renderer 仍保持整数 logical presentation 与 nearest-neighbor，因此该
修正只避免 macOS 对低分辨率窗口表面的二次放大，不伪造 framebuffer 分辨率。

## Risks / Trade-offs

- [快速大步输入可能让视觉在短时间跨过多张卡] → 保留准确目标与可中断追踪，使用固定时长单调 ease-out 并增加大 `turn` 与连续帧测试；不牺牲逻辑输入来掩盖速度。
- [固定时长曲线在大步输入时会提高瞬时速度] → 逻辑目标仍完整保留，渲染继续围绕当前视觉位置绘制固定冗余 slot；以大 `turn`、反向 retarget 和几何诊断测试约束行为。
- [App Cover renderer 可能提交非法或越界几何] → 只传递正尺寸完整 bounds，内置 renderer 选择严格同尺寸的 profile bitmap；Launcher 对相邻卡片使用静默显式 clip，双 profile 对每个 App/状态统计 InvalidGeometry、ClippedGeometry 与 FullyClipped 为零。
- [四张双 profile 1-bit Cover 增加约 45 KiB flash] → 默认实现保持现有 App 源兼容，renderer 无状态/无分配；以 firmware size audit 量化实际增量，若超出预算再调研无损压缩，而不牺牲画面完整性或运行时 RAM。
- [Settings 增加一行会压缩 320×170 布局] → 行布局按可用高度计算紧凑 step，并用最长文案、两个 profile 和裁剪诊断测试验证。
- [方向会在进程重启后恢复默认纵向] → UI 和文档明确沿用当前会话设置语义；持久化作为独立平台 change 调研。
- [全 framebuffer renderer 仍会重绘屏幕] → 本 change 先消除无意义的背景时间动画并限制绘制复杂度；局部刷新需要 Presenter/dirty-region 的独立架构工作。
- [自动快照不能证明 Memory LCD 真机观感] → 完成 host/build gates 后保留真机 30 FPS、闪烁、拖影和握持方向的明确手工验收项，不以模拟器结果冒充硬件结论。

## Migration Plan

1. 先添加 core orientation/command/snapshot 与 service tests，默认值为 `Vertical`，保持现有启动外观方向。
2. 增加静态 Cover 与默认 fallback 契约，为四个内置 App 离线生成并嵌入原创 1-bit Cover，以双 profile 几何诊断、转换可复现性和交互前后像素一致性测试锁定边界。
3. Settings 增加方向行和传播测试，再替换 Launcher 状态、轻量 chrome 与轨道渲染。
4. 更新双 profile 快照、交互/E2E 和文档基线，完成 host、desktop、firmware-compatible 构建验证。
5. 若需回滚，可删除 Cover override、新 Settings 行与 command，并恢复旧 Launcher render；没有持久数据迁移。

## Open Questions

- 方向和其他系统设置何时跨重启持久化，应由后续统一 preference-store 调研决定。
- 外部可安装 App 的 bitmap 资源何时引入，仍需独立设计资源发现、内存、缩放和许可证契约；逐帧 launch animation 必须与静态 Cover 分离。
