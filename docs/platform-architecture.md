# Cadenza OS 平台架构（原型 v0.1）

当前目标不是把三个 Demo 做完整，而是确定一套小应用可以长期生长的稳定边界。Demo 只负责证明平台契约确实可用。

## 运行链路

```text
platform callbacks ──→ bounded typed PlatformEvent queue
                              ↓ beginFrame
raw input ──→ InputReducer ──→ frozen SystemSnapshot + AppUpdateContext
                              ↓ typed SystemCommand queue
                         SystemServiceHost commit
                              ↓ same-frame AppRenderContext
                         AppRuntime + transition
                              ↓
                       canonical MonoFramebuffer
                              ↓
               T-Embed / SDL / headless / future Sharp

ES7210/I²S1 ──→ 48 kHz voice coordinator ──┬─→ analyzer snapshot ─→ Apps
                                           └─→ UAC packetizer ───→ macOS
SoundCue ─────→ 44.1 kHz sound service ───────→ I²S0 / SDL / headless
monotonic now ─→ TimerService deadline ─→ snapshot / critical TimerAlert
```

## 平台负责什么

- 初始化硬件和维护稳定的帧循环；
- 将旋钮和按钮统一成与具体 GPIO 无关的 `InputFrame`；
- 组合 `cadenza_core`、`cadenza_apps`、`cadenza_system` 与平台 adapter；
- 注册应用、维护当前应用，并调用生命周期；
- 拦截“长按打开 System Menu”这样的系统手势；
- 绘制统一的应用切换转场；
- 提供 row-major、MSB-first、`1 = black` 的 1-bit 画布，禁止应用依赖彩色
  TFT 或 SDL 特性；
- 将显示提交与应用绘制分离；
- 拥有 `InteractionSoundService`，把 Navigate、Confirm、Back 等语义提交给
  独立音频消费者；应用不得操作 SDL、I²S、WAV 路径或任意 oscillator。
- 拥有 voice capture 生命周期、consumer fan-out、隐私状态与错误降级；App 不得
  取得 raw PCM、codec、DMA、USB、Wi-Fi 或 BLE callback。
- 拥有单一 `TimerService`、单调 deadline、后台 indicator 与 critical alert；App
  不读取平台时钟，也不在自身 `dt` 中累计倒计时。

## 应用负责什么

每个内置应用只实现五个很小的接口：

```cpp
const char* name() const;
void onEnter();
void onExit();
void update(const AppUpdateContext& context);
void render(MonoCanvas& canvas, const AppRenderContext& context);
```

update context 只含 `dt`、`InputFrame`、只读 `SystemSnapshot`、catalog view、
`AppNavigator` 与绑定当前 App 身份的 `AppCommandPort`；render context 只含只读 snapshot。
应用不读取 GPIO，不直接持有屏幕，不自行实现页面切换，也不通过 `delay()` 控制
节奏。状态只属于应用实例；系统可以随时离开它，再通过 `onEnter()` 重新进入。

## 模块与所有权

- `cadenza_core`：App 契约/目录/生命周期、输入、图形、动画、声音语义；不知道
  bundled App 与平台 SDK。
- `cadenza_apps`：Launcher、Clock、Motion、Settings、Gallery；只能依赖窄 App
  context。
- `cadenza_system`：帧事务、权威系统状态、sound/voice services、固定队列、USB
  packetizer 与 DMA normalizer；不含平台 header。
- platform roots：headless、SDL、当前 Arduino firmware、候选 ESP-IDF firmware；
  负责硬件/API 对接，不重新定义业务状态或 PCM 语义。

System Overlay 属于 `cadenza_core` 的 portable presentation state，不是隐藏 App。
它只允许一个 interactive surface，按 App/transition、transient、interactive、
persistent/critical 的固定顺序合成。System Menu 打开时复用 Runtime 已有 framebuffer
冻结 active App；AppId 与 enter/exit 生命周期不变，system frame transaction 继续。
按钮从 long-press 到 release 锁存给 System，避免打开/关闭边界输入泄漏。

## 关键决定

### 输入采样不绑定帧率

主循环每次迭代都采样编码器，事件累积到下一帧再交给应用。这样即使某帧传输较慢，也不会因为只在 60 FPS 的绘制点读取 GPIO 而轻易丢失旋转边沿。

### 先做静态注册

应用目前随固件编译并在 `setup()` 中注册。这个选择让生命周期、内存占用和崩溃边界更容易验证。SD 卡资源、脚本应用和动态加载等到平台契约稳定后再做。

### 唯一权威画面是 1-bit framebuffer

T-Embed 虽然是彩屏，应用拿到的仍然只有黑白图元接口。U8g2 的精选 raster
与字体代码只写入 caller-owned framebuffer；TFT、SDL、PNG、GIF 和测试都只
读取同一份像素。Sharp profile 已在桌面和快照中验证，未来只需增加 presenter，
不再替换画布或 App 代码。

### 两块屏幕使用各自的原生分辨率

T-Embed 后端使用 320 × 170，Sharp 后端使用 400 × 240。平台不对整幅画面做非等比缩放；应用从 `MonoCanvas` 读取实际宽高，按区域、边距和锚点响应式布局。这样既不会把 Sharp 的像素构图锁死在临时硬件上，也能充分利用 T-Embed 的整块屏幕。

### 动画与内存边界

Tween、Sequence、Parallel、Timeline、Spring、粒子和状态机都使用值语义或
固定容量，不在逐帧路径分配。SDL3、stb 和 gif-h 只属于 desktop adapter；
core 不包含 Arduino、TFT_eSPI 或 SDL header。转场采用明确的 source/
destination framebuffer 所有权，代价见 [`memory-budget.md`](memory-budget.md)。

### 音频时钟不绑定图形帧

App 与 Runtime 只在状态实际变化时提交 `SoundCue`。16 项固定 SPSC 队列把
主线程与音频消费者隔离；Navigate/Boundary 最多占 12 项，为关键提示和控制
保留容量。Muted/StopAll 另有原子安全邮箱，因此即使关键 cue 填满队列也不能
阻塞静音。

`AudioEngine` 由唯一消费者拥有：桌面是 SDL3 AudioStream callback，T-Embed
是固定到 core 0 的 FreeRTOS task，headless 测试同步拉取。三者消费相同的
44.1 kHz、S16、mono PCM。T-Embed adapter 只负责将 mono 复制为 right-left
I²S frame；任何平台 adapter 都不得另写一套音色。

当前音色是集中在 `sound_cue_library.cpp` 的 16 项参数合成 palette。四声部
`AudioEngine` 提供 wavetable Sine、预计算指数包络与二次谐波；固定容量的
consumer-owned event 调度器支持 sample-offset 延迟，Muted/StopAll 会同时清空
当前声部和未来 event。它不是不可替换的产品资产；未来 WAV 仍映射到相同
`SoundCue` Event，母版、平台转换、来源权利和真机验收遵循
[`audio-asset-contract.md`](audio-asset-contract.md)。

### 帧事务与异步回调

Wi-Fi、BLE、USB、I²S 等 callback 只能写入有界 adapter/mailbox 或
`PlatformEvent` queue；它们不能调用 App 生命周期或直接修改 snapshot。
SDK callback 使用单 producer/单 consumer 的 `PlatformEventMailbox<N>`，满时
拒绝 newest 并记录 posted/consumed/rejected/high-water；主 system loop 再将
Wi-Fi/Bluetooth LE 等 typed event 摄取进 host。App 只看到稳定的
`ConnectivitySnapshot`，不看到 mailbox、SSID/凭据、socket 或 GATT callback。
`beginFrame()` 在预算内摄取事件并冻结 update snapshot，App update 只排队命令，
`commitCommands()` 按 FIFO 提交并生成同帧 render snapshot。队列满、拒绝原因和
high-water 都可观测，避免把中断/SDK 线程变成隐式重入路径。

Timer 使用同一 frame transaction，但时间真相来自显式 `beginFrameAt(nowMs, dt)`：
firmware、SDL 和 headless 分别注入各自 64-bit 单调时间。动画 `dt` 为零或被 clamp
不会暂停 Timer；只有 Pause 命令会。到期 edge 在 begin phase 产生一次 bounded
alert 请求，App command 仍在 commit phase 生效。

### App 能力路由不是进程沙箱

组合根用固定 `AppDescriptor`/`AppCapabilitySet` 声明每个 App 的最小能力；未声明
即默认拒绝。Runtime 只把绑定当前 AppId 的 operation port 放进 update context，
App 不能通过该 API 自选 caller。port 在入队前检查 capability，SystemServiceHost
在 commit 时通过只读 catalog resolver 再检查一次，因此错误 adapter 直接注入 App
信封也不能绕过策略。System 与 USB 使用不同 owner，拒绝原因、owner、operation 和
queue high-water 可审计。

这是同一地址空间内的 policy enforcement，不是 native process sandbox：C++ 固件
中的恶意或内存越界代码仍可能绕过类型边界。当前目标是约束受支持 App API、减少
误用并提供可验证审计；若未来加载不可信第三方代码，需要 MPU/process/WASM 等另一个
隔离层，不能把 capability set 宣称为安全沙箱。

### 系统资源使用租约

网络、BLE advertising/scanning 与 voice analyzer 使用固定容量 owner-aware lease，
而不是“最后一次 bool 写入获胜”。Foreground lease 在 App 实际完成 transition swap
的同一帧、command commit 前自动回收；Session 由会话结束释放；Persistent 仅允许
受信 System/平台 owner，普通 App 会被拒绝。资源 desired state 从 owner count 推导，
所以一个 App 退出不会错误关闭另一个 owner 正在使用的资源，最后一个 owner 释放后
才停止硬件。acquire 幂等，unknown release、容量失败、自动清理与 high-water 都有
计数器。

### Voice session、隐私与 backpressure

capture coordinator 固定输出 48 kHz、S16、mono、480-sample/10 ms blocks。
Analyzer 与 USB 各有容量 4 的 SPSC queue；慢 consumer 只丢自己的 newest block，
不阻塞采集也不破坏另一个 consumer。USB 再切成 48-sample/1 ms packets，underflow
发等长静音，disconnect/alt inactive 清除 stale data。任一 consumer active 时全局
显示 `MIC`；USB streaming 时默认抑制系统 cue，避免扬声器被板载麦克风回录。

T-Embed 候选输入由 ES7210/I²S1 adapter 独占，输出由 I²S0 adapter 独占；两者不得
共享 task、DMA 或可变 buffer。当前自动化覆盖 portable 语义和各自构建，真实并发、
左右通道、增益、时钟和声学泄漏仍是到货后的硬件门禁。

## 后续平台工作

1. 增加应用级持久化存储命名空间；
2. 根据真机测量决定是否复用 Gallery/Runtime 的离屏 framebuffer；
3. 实现 Sharp presenter，并在实体屏上对比 400×240 快照；
4. 根据实体 T-Embed 试听选择默认音色，并在需要时实现 sample voice；
5. 定义休眠、背光/对比度等系统服务，而不是让应用直接操作硬件。
6. 为 Wi-Fi provisioning、凭据、BLE service/RF coexistence 单独建立 OpenSpec，
   复用 platform-event → service state machine → snapshot/command，不暴露 raw socket
   或 GATT callback。
