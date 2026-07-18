## Why

Cadenza 已经拥有统一的 1-bit 视觉、输入语义和动画反馈，但交互仍然是无声的，旋钮、确认、返回和应用转场缺少构成“完整数字物件”的听觉触感。现在输入与动画边界已经稳定，适合参考 Playdate 的克制声音哲学，建立可移植、可关闭、可验证的系统声音能力，而不是让各 App 直接操作扬声器或散落播放资源。

## What Changes

- 建立 Playdate 与嵌入式交互音频调研基线，明确 Cadenza 的声音原则、语义词汇、响度/重复控制和真机调音方法。
- 新增与平台无关、固定容量、逐样本无动态分配的单声道声音核心，支持 triangle、带限 square、noise、包络、音高变化和有限复音。
- 新增固定容量语义命令队列；图形主线程只提交 cue，SDL callback 与 ESP32 audio task 独占合成器状态并持续供给设备，避免慢帧造成欠载。
- 将选择、确认、返回、切换、边界与拒绝等系统语义映射为统一声音提示；App 表达语义，不直接依赖音频设备。
- 为 SDL 桌面模拟器提供 AudioStream callback 输出，为原版 T-Embed 的 MAX98357A 提供独立 task + I²S DMA 输出，并为 headless 测试提供可观测而无声的后端。
- 在 Settings 中提供系统音效开关与分级音量；静音时所有非必要交互音立即保持无声，声音永远不是唯一反馈渠道。
- 增加声音核心、语义路由、速率限制、静音、桌面集成、固件编译和确定性渲染验证。

## Capabilities

### New Capabilities

- `portable-audio-core`: 固定容量的可移植声音命令、带限渲染、三声部/抢占、确定性与实时安全契约。
- `interaction-sound-language`: Cadenza 系统交互声音语义、Playdate 风格约束、重复抑制、音量/静音、视觉等价反馈与 App 接口。
- `audio-platform-output`: headless、SDL 与原版 T-Embed I²S 音频后端、生命周期、缓冲和跨平台验证要求。

### Modified Capabilities

无。当前主规格尚未包含声音契约；本变更只新增能力，并复用现有输入、Runtime、Settings 与桌面模拟器边界。

## Impact

- `cadenza_core` 将新增音频核心和系统声音服务，并在 `AppRuntime`/内置 App 的语义动作处接入。
- `cadenza_desktop` 与 SDL 可执行程序将新增音频设备适配，headless runner 增加确定性声音观测能力。
- T-Embed 固件将使用 GPIO 7/5/6 驱动板载 MAX98357A，以 stereo I²S frame 承载复制后的 mono PCM；不引入大型音频解码依赖。
- Settings、CMake、PlatformIO、测试与开发文档会扩展；现有用户未提交的文字布局变更必须保持兼容。
