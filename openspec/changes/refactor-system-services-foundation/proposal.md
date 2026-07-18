## Why

Cadenza 已经拥有可移植图形、输入、动画和提示音，但 App 直接依赖完整 `AppRuntime`，内置 App 标识也硬编码在 core 中；继续把 Wi-Fi、Bluetooth LE、麦克风和语音传输加入这条边界，会让 Runtime 变成难以测试、难以并发隔离的全局 service locator。现在设备目标进一步明确为“内置 App 可使用语音输入，同时可作为 Mac 的语音输入外设”，需要在引入更多平台回调、凭据和实时流之前建立可审计的系统服务与音频输入边界。

## What Changes

- **BREAKING** 用窄化的 App update/render context 替代向 App 暴露完整 `AppRuntime`；App 只读取稳定的系统快照并提交类型化命令，不直接持有平台服务或异步 callback。
- **BREAKING** 将内置 `AppId` 从 runtime core 移出，改为由组合根注册的值类型标识与可配置 Home App；新增系统 App 不再修改 runtime 公共枚举。
- 新增固定容量、无逐帧分配的 `SystemServiceHost`：在每帧明确摄取平台事件、冻结快照、运行 App、执行命令并发布下一份状态；容量耗尽与拒绝行为可诊断。
- 将提示音、音量和 motion 设置从 `AppRuntime` 移入系统服务，保持现有视觉、声音、转场、快照与 PCM 行为。
- 将 bundled Apps/Gallery 与 portable runtime target 分离，固件、headless 和 desktop 组合根显式装配相同的 App 与系统服务。
- 新增平台无关的麦克风/语音输入基础：固定格式 PCM frame、采集状态与健康快照、单一硬件采集所有权、固定容量 fan-out/consumer 边界，以及 App 与外部传输之间可观察的会话仲裁。
- 新增 T-Embed ES7210 + I²S 麦克风适配和 headless 确定性输入；App 可以通过系统快照和命令使用语音活动/电平能力，不接触 I²S、codec 或 DMA。
- 新增面向 macOS 的 USB Audio Class 麦克风输出路径和描述符/流契约；USB 是首个系统级语音外设传输，BLE/Wi-Fi 首阶段用于连接、控制和未来 App 服务，不冒充传统蓝牙耳麦。
- 评估并锁定支持 TinyUSB UAC 的固件 SDK/构建基线；保留桌面 CMake 和 portable core，不以未维护的私有补丁强行扩展当前 Arduino 2.0.17 预编译 TinyUSB。
- 补充系统服务顺序、命令拒绝、快照一致性、任意 App 注册、麦克风 backpressure、USB packet cadence、组合根一致性和 firmware 构建回归；没有实体设备时明确区分自动化完成与 Mac 枚举/录音真机验收。

## Capabilities

### New Capabilities

- `system-service-runtime`: 类型化系统命令、稳定快照、平台事件摄取、帧内执行顺序、固定容量与诊断契约。
- `app-capability-contract`: 窄化 App context、非硬编码 App 标识、可配置 Home App、App catalog 与组合根职责。
- `voice-input-core`: 平台无关 PCM 输入、采集状态、语音活动/电平、固定容量传递、consumer 会话与 backpressure 契约。
- `voice-platform-input`: T-Embed ES7210/I²S 采集、headless 输入、生命周期、格式转换、错误降级与真机证据边界。
- `usb-voice-device`: USB Audio Class 麦克风、macOS 枚举语义、音频 packet 时钟、CDC 兼容、断连/静音与验证要求。

### Modified Capabilities

- `portable-runtime`: App 生命周期不再暴露完整 Runtime，应用注册不再依赖硬编码 built-in enum，Runtime 不再拥有平台系统服务。
- `interaction-sound-language`: App 和 Runtime 通过系统命令提交 cue/音量，权威音量状态来自系统快照。
- `audio-platform-output`: 音频消费者从系统服务宿主取得提示音输出，生命周期必须与麦克风输入和 USB 语音传输共存。
- `runtime-verification`: 增加系统服务、任意 App 注册、麦克风输入和 USB voice device 的 host/firmware/实体设备分层证据。

## Impact

- 主要影响 `App`/`AppRuntime` 公共 API、`cadenza_core` 源码清单、bundled Apps、headless/desktop/firmware 组合根、提示音消费者和全部 App 测试。
- 新增可移植 system/voice 模块、T-Embed microphone adapter、USB UAC adapter、对应 host fakes、协议测试与研究/采纳决策文档。
- 固件构建基线可能从 PlatformIO Arduino 2.0.17 / ESP-IDF 4.4.7 迁移到受支持的 ESP-IDF + Arduino component 组合；正式定案以可复现 build spike、TFT/输入/音频回归和依赖许可证为准。
- ESP32-S3 只支持 Bluetooth LE；本 change 不承诺 Classic Bluetooth HFP/A2DP，也不把普通 BLE GATT 数据流声明为 macOS 系统麦克风。
- 现有 1-bit framebuffer、光栅、动画数值、转场、SoundCue 音色和两种 profile 的批准画面保持不变。
