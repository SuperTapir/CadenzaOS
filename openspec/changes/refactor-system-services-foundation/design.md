## Context

Cadenza 当前已经形成可移植的 1-bit 图形、输入、动画、App 生命周期、转场和提示音核心，并通过 headless、SDL3 与 T-Embed firmware 共享源码。现有自动化基线为 host 39/39、SDL dummy smoke 与 firmware 编译通过；当前 firmware 约占 83,736 B RAM（25.6%）和 339,669 B flash（10.8%）。这些证据说明图形与交互原型可以继续演进，但不能证明 Wi-Fi、Bluetooth LE、麦克风或 USB Audio 的真机行为。

当前边界有三个结构性问题：

- `App` 的 update/render 直接获得完整 `AppRuntime`，提示音、音量、motion 和导航等能力继续加入后会形成 service locator，并允许 App 在任意阶段改变全局状态。
- `AppId` 把 Launcher、Timer、Motion、Settings、Gallery 固化为 core 枚举，bundled App 实现也编入 `cadenza_core`，新增小应用需要修改平台核心。
- 未来 Wi-Fi、BLE、I²S 和 USB 回调具有不同线程、时钟与 backpressure 语义；如果 callback 直接进入 App 或 Runtime，将破坏当前单线程、确定性 frame 契约。

产品目标进一步明确为：设备内部 App 可以使用语音输入能力，同时设备可以连接 Mac 并成为系统可选的麦克风。ESP32-S3 只支持 Bluetooth LE，不支持 Classic Bluetooth，因此不能把 HFP/A2DP 耳麦路径作为产品假设。普通 BLE GATT 或 Wi-Fi 音频也不会自动成为 macOS 系统麦克风；首个系统级外设路径应为 USB Audio Class，BLE/Wi-Fi 首阶段承担配网、控制、状态与未来 App 服务。

当前没有实体 T-Embed。设计与实现必须把 host 可证明、firmware 可编译和实体设备可验收分开，不能以模拟输入、descriptor 单测或成功编译代替 Mac 枚举、真实录音、时钟稳定性、射频共存和功耗证据。

### 调研基线与采纳边界

以下源码版本、许可证与直接相关实现已锁定；正式实现中的研究记录 SHALL 保存关键文件、实验命令和采用结论：

| 参考 | 锁定版本 | 许可证 | 阅读重点 | 结论 |
|---|---|---|---|---|
| Zephyr zbus | `da9b7b9d257b71098ed2c0d156d137420ea86779` | Apache-2.0 | typed channel、同步/异步 observer、静态容量、subscriber 覆盖语义 | 采用 typed message、显式 delivery mode 与静态容量；不采用全局 bus，避免发布者线程执行、重入和 latest-value 覆盖造成的隐式丢失 |
| Matter/CHIP | `8d8de11a5e84415237bfd42a8fe87cc448d3de62` | Apache-2.0 | `PlatformManager`、`ChipDeviceEvent`、`ConnectivityManager`、FreeRTOS event loop | 采用平台 callback 入队、主 system loop 推进状态机；不引入其大型协议栈和全局 singleton |
| Pigweed `pw_async2` | `1cc1c28fc56f6f65d7d0bcd6945d522d1b9f10e0` | Apache-2.0 | 固定容量 channel、cooperative dispatcher、simulated time | 采用有界 backpressure 与模拟时间思想；当前不引入新调度器和工具链依赖 |
| ETL message bus | `7f290365a9f1b72bf89b4b77ec7dbe39b228bd8e`（20.48.1） | MIT | 固定容量 message/router、同步 broadcast | 仅借鉴无堆分配 typed dispatch；不采用全局同步 bus 和 type-erased route |
| ESP IoT Solution UAC | `9971a4692b5c50fbe055db786a9bd6f541372b6e` | Apache-2.0 | `usb_device_uac` 1.3.1、双缓冲、约 1 ms cadence、MIC-only 修复、多声道枚举修复 | 作为 ESP-IDF 5.x UAC adapter 候选；必须复现 MIC-only build、descriptor 与时钟/杂音实验，不把示例成功等同产品完成 |
| TinyUSB | `aa410008e8e74b0727f8c30a1ec109ff2c37efc6` | MIT | `cdc_uac2` composite example、UAC2 descriptor、44.1/48 kHz streaming | 采用 UAC2 + CDC composite 结构和固定 48 kHz mono input；通过受支持的 SDK/component 集成，不私补旧预编译库 |
| LilyGO T-Embed | `17867865b2d157a3a40ebe88e6ad3f0ba113aafd` | MIT（复制前仍检查文件级声明） | ES7210、I²S1、GPIO、双 MEMS 麦克风示例 | 采用经板级配置注入的 ES7210/I²S adapter；参数先锁定示例可支持范围，真机到货后验证通道、增益和时钟 |
| 当前 Arduino-ESP32 | `5e19e086c43d0fa5e5a596497ff8f11a0a43f6c2`（2.0.17） | LGPL-2.1 | 预编译 TinyUSB 配置、legacy I²S | 保持为迁移前基线；其配置未启用 `CFG_TUD_AUDIO`，不在私有 patch 上建设长期 UAC |
| 当前 ESP-IDF | `38eeba213aa695aabfd6d89aa9f5078dbe5a94c3`（4.4.7） | Apache-2.0 | USB/I²S/build 基线 | 仅用于现状回归；UAC candidate 要求可复现的受支持 IDF 5.x baseline |

已知失败案例包括：ESP IoT Solution UAC 曾修复 MIC-only 路径构建和 Windows 多声道 endpoint size 导致的 Code 10；另有 ESP32-S3/IDF 5.3/5.4 随机杂音报告。它们说明 descriptor 静态正确并不足够，USB packet cadence、采样时钟漂移、underflow、buffer ownership 和真实主机长录音都必须独立验证。

## Goals / Non-Goals

**Goals:**

- 把 portable runtime、system services、bundled Apps 和 platform adapters 变成单向依赖层，新增 App 不再修改 core 枚举。
- 为 App 建立只读稳定快照、类型化命令和明确 frame transaction，不暴露异步 callback、driver 或完整 Runtime。
- 建立固定容量、无逐帧堆分配、可诊断拒绝的 system service host。
- 建立一个麦克风硬件 owner、多个独立 consumer 的 portable voice input core，App 分析与 USB streaming 可以并存且互不阻塞。
- 用固定 48 kHz、S16、mono 的 UAC2 microphone + CDC composite 作为 Mac 首个系统输入路径。
- 在无硬件阶段完成 host 行为、descriptor/packet、headless replay、SDK build spike 和 firmware 回归，并保留明确的实体设备验收清单。

**Non-Goals:**

- 本 change 不交付 Classic Bluetooth HFP/A2DP、Bluetooth LE Audio、Wi-Fi 虚拟声卡或 Mac 驱动/常驻代理。
- 本 change 不向普通 App 暴露原始 I²S DMA 指针，也不交付录音文件系统、云语音识别、唤醒词或完整 DSP 框架。
- 本 change 不承诺 Wi-Fi/BLE 产品 UI、配网协议、凭据存储和 OTA；只保证未来连接服务能复用相同 App command/snapshot 边界。
- 本 change 不改变现有 1-bit 视觉语言、转场数值、SoundCue 音色和批准 snapshots。
- 没有实体设备时不宣称 Mac 枚举、麦克风音质、延迟、长期时钟稳定、RF coexistence、热量或功耗通过。

## Decisions

### 1. 分层目标与依赖方向

目标层次为：

```text
composition roots (headless / SDL / T-Embed)
        ↓ assemble
cadenza_apps          platform adapters
        ↓                    ↓ events / I/O
cadenza_system  ← typed ports / snapshots
        ↓
cadenza_core
```

`cadenza_core` 只保留 framebuffer、input、animation、transition、App 生命周期和窄接口；`cadenza_system` 实现 service host、settings、sound 与 voice session；`cadenza_apps` 包含 bundled Apps 和 Gallery。平台 adapter 只实现 time/input/display/audio/microphone/USB 等端口，组合根显式装配。

备选方案是保留单一库，仅用目录区分。该方案无法由链接依赖阻止 App/core 反向引用，因此不采用。

### 2. App 使用窄 context，而不是 Runtime 或全局 event bus

App update context 包含 `AppCatalog`、`AppNavigator`、只读 `SystemSnapshot` 和 `SystemCommandSink`；render context 只包含渲染所需 catalog/snapshot。具体命名可在实现中按现有风格调整，但职责不得重新合并为 Runtime facade。

System command 使用封闭的类型集合和固定容量 FIFO。命令提交返回 accepted/rejected，拒绝原因进入诊断；App 不获得 service 引用。平台 callback 先进入 adapter 自有线程安全队列，再由主 loop 摄取，不允许 callback 直接调用 App/system service。

备选的全局 pub/sub bus 被否决：同步 observer 容易在发布者线程重入，latest-value channel 可能覆盖尚未消费状态，异步 subscriber 的 delivery/backpressure 难以从调用点审计。这里采用显式 command + snapshot，跨层数据流可静态追踪。

### 3. 每帧是一个可测试 transaction

每帧顺序固定为：

1. `SystemServiceHost` 摄取有界平台事件、推进服务并冻结本帧 `SystemSnapshot`。
2. `AppRuntime` 使用冻结快照更新 active App；App 只能提交命令。
3. service host 按 FIFO commit App commands，更新 authoritative state 和下一份/渲染可见快照。
4. Runtime 使用 commit 后快照渲染，平台消费输出。

Settings 的音量文字因此可以在确认操作后的同一帧 render 显示新值，但 App 在 update 中不能重入观察自己刚提交的写入。超过 per-frame event/command budget 的输入保留或按各 service 明确策略拒绝，并累计诊断，不能无限循环饿死渲染。

备选方案包括“命令下一帧才可见”和“提交时立即调用服务”。前者给设置 UI 带来额外延迟，后者允许重入和调用顺序依赖；均不采用。

### 4. AppId 为值类型，catalog 与 Home App 由组合根配置

`AppId` 改为有 invalid 值的窄整数 value type；built-in id/constants 移到 `cadenza_apps`。Runtime 使用固定容量 registry，拒绝重复、invalid 或超容量注册。组合根显式配置 Home App；system long-press 导航到该 id。App 标题/工厂/图标等通过 catalog 查询，不通过 core switch。

这保持嵌入式静态容量，也允许新增小 App 而无需修改 core。动态插件加载、堆分配 registry 和 ABI 稳定插件系统不在本阶段范围。

### 5. voice capture 是单 owner、多 consumer 的固定格式流

portable capture 格式固定为 48,000 Hz、signed 16-bit、mono。基础 block 为 10 ms（480 samples），便于电平/VAD/测试；USB adapter 从自己的 consumer queue 切成 1 ms cadence（48 samples），不得要求硬件 producer 按 USB callback 形状写入。

一个 capture coordinator 独占 ES7210/I²S 生命周期，并根据 active consumer 集合启动/停止硬件。每个 consumer 拥有独立固定容量 SPSC queue；producer 不等待 consumer。队列满时该 consumer 丢弃最新 block并增加 overrun，不影响其他 consumer，也不允许覆盖正在读取的 slot。

首批 consumer 为：

- analyzer：产生 App 可读的 availability、active、RMS/peak、简单 VAD、overrun/error 快照；App 通过 start/stop intent 控制分析会话。
- USB microphone：独立消费 PCM；host 未请求 stream 时不累积待播历史，激活时清空 stale blocks。

不使用一个共享消费队列，因为慢 USB/分析任务会彼此偷取或阻塞数据；不把原始 PCM 放入 60 Hz `SystemSnapshot`，避免复制、时钟混淆和 App 卡住实时 producer。未来确需录音/识别时新增显式 raw-stream consumer port 和资源预算，而不是扩张通用 App context。

### 6. 会话、隐私和音频共存语义显式化

mic hardware 状态至少为 unavailable/stopped/starting/running/error；consumer intent 与硬件状态分开。任一 consumer active 时系统快照必须显示麦克风使用状态，UI 必须有持续视觉隐私指示，错误和 overrun 可诊断。

USB external microphone streaming 与 App analyzer 允许并存。为避免扬声器提示音被板载麦克风重新录入，USB stream active 时默认抑制/duck 系统 cue，但保留视觉反馈；具体衰减策略可配置并由 host test 固定。输出 I²S0 与输入 I²S1 在 T-Embed 上分别拥有 task/DMA，不共享可变 buffer。

备选的“USB 与 App 互斥”实现简单但削弱平台能力；备选的“始终播放 cue”会产生明显 acoustic leakage。采用共享 capture + 独立 consumer，并默认安静模式。

### 7. USB 采用 UAC2 microphone + CDC composite

首版 descriptor 暴露一个固定 48 kHz、S16、mono input terminal 和 CDC 控制/日志接口。选择 UAC2 是因为 macOS 有内置 class driver、无需自定义驱动，同时 TinyUSB/ESP 方案已有 composite 参考；不使用 Audio Class 4。ESP32-S3 endpoint 数量必须在 descriptor test 中计算并保持在硬件上限内。

USB adapter 每 1 ms 提供 48 samples；队列 underflow 时发送同长度 silence 并累计 counter，不能重复旧音频。disconnect/alt-setting inactive 时停止消费；重新 active 时 flush stale samples。descriptor、packet size、interface/endpoint 唯一性和 MIC-only 配置必须在 host/SDK tests 中覆盖。

Wi-Fi 音频需要 Mac app/虚拟设备，BLE 普通 GATT 也不是系统麦克风，因此不作为首条路径。BLE/Wi-Fi 后续 service 仍必须使用 platform-event → service state machine → snapshot/command 契约。

### 8. 固件 SDK 先 spike，再迁移，不私补 Arduino 2.0.17

当前 PlatformIO Arduino 2.0.17 基于 ESP-IDF 4.4.7，预编译 TinyUSB 配置没有启用 audio class；直接 patch framework 会形成不可维护的隐式 fork。隔离 spike 已证明官方 ESP-IDF 5.5（`8c750b088c7cd857d079c0eeb495da199b359461`）可与 Arduino component 3.3.10、`usb_device_uac` 1.3.1 和 TinyUSB 0.19.0~3 组成 UAC2 microphone-only + CDC，并保存 sdkconfig、完整依赖 lock、descriptor 和 size/task/stack 结果。因此采用该组合作为迁移候选；现有 target 继续回归，display/input/I²S output/microphone 的完整 candidate 组合仍是切换门禁。

只有 spike 满足以下门槛才切换主 firmware baseline：

- TinyUSB UAC2+CDC descriptor 与 MIC-only target 可编译；
- 当前 TFT/input/audio output adapters 有明确迁移结果；
- flash/RAM/task/stack 报告可比较，且保留安全余量；
- host/CMake 不受 SDK 迁移影响；
- 依赖版本、许可证与回滚方式被记录。

若 spike 失败，core/system/voice host 实现仍可合并，hardware UAC task 保持未完成并记录 blocker；不得用不可复现的本机 framework 修改绕过。

### 9. 验证证据分三层

自动化层包括 core/system unit tests、deterministic PCM replay、queue overflow/underflow、session arbitration、frame ordering、descriptor/packet tests、SDL/headless E2E、sanitizer 和 strict warning。构建层包括原 firmware 回归与候选 SDK UAC build、size/task/stack 报告。实体层在真机到货后执行 Mac System Information/Audio MIDI Setup 枚举、输入选择、至少 30 分钟录音、重连/睡眠唤醒、App+USB 并发、cue 泄漏、幅度/噪声/通道/时钟漂移、Wi-Fi/BLE coexistence 和功耗测试。

每个完成声明必须标注证据层。实体层未执行时，change 可以达到“host foundation complete / hardware validation pending”，不能 archive 为完整产品能力。

## Risks / Trade-offs

- [Risk] 一次拆分 Runtime、Apps 和 sound/settings 会产生较大 API 迁移面 → 先用 characterization tests 固定生命周期、snapshot、PCM hash 和转场，再按 composition root 逐步迁移；每步保持 host tests 可运行。
- [Risk] 固定容量过小会丢命令或 PCM，过大会浪费嵌入式 RAM → 所有容量集中配置并暴露 high-water/overrun/reject 计数；用 burst tests 和 firmware map 校准。
- [Risk] snapshot commit 后同帧 render 可见的双阶段语义较复杂 → 用单一 frame coordinator 封装顺序并写 trace/golden test，禁止 App 自行调用 commit。
- [Risk] 简单 RMS/VAD 不等于语音识别质量 → 本阶段只定义可测试的 level/activity primitive，不承诺复杂噪声环境准确率。
- [Risk] USB 48 kHz 与 codec clock 的微小偏差导致长时间 overrun/underflow 或杂音 → 暴露 counters，验证 clock source/feedback 需求，并以真实 Mac 长录音为发布门槛。
- [Risk] UAC2+CDC descriptor 或 endpoint 配置在不同 macOS/SDK 版本表现不同 → host descriptor lint、官方 TinyUSB example 对照、锁定 SDK，并在至少两类 Mac/OS 版本条件允许时复测。
- [Risk] SDK 升级破坏 TFT、encoder、legacy I²S output 或 PlatformIO workflow → UAC spike 与现有 firmware target 并存至回归通过；保留旧 target 作为 rollback，不原地修改 framework package。
- [Risk] 输入和扬声器并发产生 acoustic feedback/leakage → USB streaming 默认抑制 cue，真机验证物理隔离、增益与必要的进一步 duck/AEC 策略。
- [Risk] Wi-Fi/BLE 同 RF、任务和内存压力尚未测量 → 不在本 change 承诺并发产品指标；先保留 service/event 边界，真机后做 coexistence matrix 和资源测量。
- [Risk] T-Embed 上游仓库或示例许可边界不清 → 仅依据接口/引脚做独立 adapter；复制任何源码前重新确认文件级 license 和 attribution。

## Migration Plan

1. 保存现有 host、snapshot、PCM 与 firmware size 基线；新增 source-boundary 审计，防止迁移中引入平台头。
2. 引入 `AppId` value type、fixed App catalog、Home App 配置和窄 context；迁移 bundled Apps，保持生命周期和视觉 golden 不变。
3. 引入 `SystemServiceHost`、frame coordinator、typed commands/snapshot；先迁移 settings/sound/motion，再把 bundled Apps 移出 core target。
4. 以 headless producer 实现 voice core、consumer fan-out、analyzer、session 与完整溢出/时钟测试。
5. 建立隔离 SDK/UAC build spike并形成采用记录；门槛满足后新增 ES7210/I²S1 adapter 与 UAC2+CDC adapter，保持 hardware feature 可编译开关。
6. 运行 host、strict warning、sanitizer、snapshot、SDL dummy/E2E、现有 firmware、候选 firmware 和 OpenSpec strict validation；记录自动化证据。
7. 真机到货后执行设备验收清单，校准 codec/channel/gain/clock/buffer；只有实体证据通过后关闭 hardware pending 项并 archive change。

回滚以提交边界和双 firmware target 为单位：在 system-service API 迁移未完成前不删除旧 composition path；切换 SDK 前保留旧 target。不得通过恢复 App 对完整 Runtime 的依赖来修复单个迁移问题。

## Open Questions

- T-Embed 双 MEMS 麦克风在量产板上的左右通道、模拟增益、MCLK 稳定性和最佳单声道 downmix，只能真机确认。
- ESP-IDF 5.5 + Arduino component 3.3.10 已锁为迁移候选；是否切换为产品 baseline 仍由 TFT/encoder/I²S 共存编译、真机 UAC 枚举和长录音决定。
- Mac 主机对固定异步 input endpoint 是否需要显式 feedback/clock compensation，需要通过 TinyUSB 实现与真实长录音 counters 决定。
- USB stream active 时系统 cue 是完全静音还是低增益 duck，最终值需结合声学泄漏与可用性真机测试；默认实现采用完全抑制并保留视觉反馈。
- 后续 App 若需要原始语音（录音、STT、网络发送），应新增何种受权限和容量约束的 raw-stream API，将在首个真实 consumer 出现时另立 OpenSpec，不提前泛化。
