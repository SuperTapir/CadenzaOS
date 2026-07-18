## Why

现有交互音效虽然已具备稳定语义和跨平台输出，但全部 cue 共用相似的三角波/方波滑音，日常操作与重要结果缺少材质和时间层级，听感容易显得尖锐、同质和疲劳；现有词汇也只覆盖即时 UI 操作，不足以表达平台的任务结果、系统提醒和设备生命周期。经对成熟系统声音主题的源码/资产调研、多轮原创打样和上下文试听，已拍板 Semantic Hierarchy 完整 palette，并明确以用户提供的 Select、Turn On、Turn Off 分别作为 Navigate、ToggleOn、ToggleOff 的本地校准参考；正式实现使用原创参数合成近似还原其低调反馈、时长和动作轮廓，不复制或分发参考采样。

## What Changes

- 将现有单一“三角波主体 + 低增益方波” palette 替换为 Semantic Hierarchy：Navigate/Boundary 保持短促、干燥的机构反馈，Confirm/Back 使用更完整但仍克制的调音敲击尾音，Toggle 位于两者之间，Reject 保持低沉且不更响。
- 将声音语义组织为四个稳定家族：
  - **Input**：Navigate、Boundary。
  - **Action**：Confirm、Back、ToggleOn、ToggleOff、Reject。
  - **Outcome**：Complete、Warning、Failure。
  - **System**：Notification、Connect、Disconnect、PowerOn、PowerOff。
- 为 Confirm/Back 与 ToggleOn/ToggleOff 使用不同的节奏和材质结构：Confirm/Back 是跨导航层级的两段调音敲击，Toggle 是原地状态变化的单次机械 click + 短共振，不得只依赖升/降调区分。
- 新增语义只在系统确实发生对应的任务结果、提醒或生命周期事件时触发；本变更不为尚不存在的业务伪造触发点，也不使声音成为唯一反馈。
- 在 portable audio core 中增加受限的延迟起音、指数衰减与短正弦共振原语，使正式实现可近似还原已选听感；逐样本路径不使用堆、double 或昂贵通用函数。
- 扩展内部 `SoundCue` 语义词汇，同时保持命令队列、音量、冷却和 SDL/I²S 平台接口形态不变；调用方仍只提交语义，不感知波形、样本或设备。
- 为新 palette 增加可重复的 PCM/golden、包络、延迟、峰值、时长、主要频段、抢占和确定性回归；参考 WAV 仅用于本地校准，不进入仓库或运行时。
- 在修改正式 palette 代码前，先为 Input、Action、Outcome 和 System 全部 cue 生成不进入产品资产目录的原创试听样音与上下文操作 demo；只有在人工 A/B 确认语义可辨、家族一致且不疲劳后，才固定正式参数和 golden。
- 将实体 T-Embed 的小扬声器可辨识性、连续旋钮疲劳度、破音/点击和 Confirm/Back 方向辨识纳入正式验收；未完成实体验证前不宣称最终听感通过。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `interaction-sound-language`：将单一滑音音色要求修改为按交互频率和重要度分层的 Semantic Hierarchy 声音语言，并把词汇扩展到 Input、Action、Outcome 与 System 四个家族。
- `portable-audio-core`：为已选 palette 增加固定容量、无堆分配的延迟起音、指数衰减与短正弦共振合成契约。
- `audio-platform-output`：明确新 palette 在实体 T-Embed 上的疲劳度、可辨识性与输出安全验收门禁。

## Impact

- 主要影响 `SoundCue` 内部枚举、`sound_cue_library`、`AudioEngine`/`ToneSpec`、试听导出工具、相关音频测试和音频设计文档。
- `AppRuntime`、内置 App、SDL AudioStream 与 T-Embed I²S adapter 继续消费原有语义/PCM 契约，预期不需要公共 API 迁移。
- 不新增第三方运行时依赖，不引入或分发 KDE、elementary、Playdate 等参考资产，不新增背景音乐或通用合成器 API。
