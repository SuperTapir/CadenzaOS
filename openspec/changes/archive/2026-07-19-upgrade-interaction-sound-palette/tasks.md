## 1. 校准与失败用例

- [x] 1.1 记录 Select、Turn On、Turn Off 的本地非版权校准特征，并定义“不天差地别”的时长、包络、频段与上下文试听边界
- [x] 1.2 先添加 Sine、指数衰减、延迟 event、清空 delayed 状态和非法参数失败测试
- [x] 1.3 先添加 15 项 SoundCue 名称、定义、确定性 PCM、结束静音和家族特征失败测试

## 2. Portable 合成与调度

- [x] 2.1 为 AudioEngine 实现无昂贵逐样本函数的 wavetable Sine 与预计算指数衰减
- [x] 2.2 扩展 ToneSpec decay 与 SoundEvent delay 契约，并在 InteractionSoundService 实现固定容量 consumer-owned delayed event 调度
- [x] 2.3 保持四声部优先级、平滑抢占、Muted/StopAll、队列饱和和 shared-source 实时安全契约

## 3. Semantic Hierarchy Palette

- [x] 3.1 扩展 SoundCue 为 Input、Action、Outcome、System 四家族 15 项语义并保持现有调用方兼容
- [x] 3.2 实现 Navigate、Boundary、Confirm、Back、ToggleOn、ToggleOff、Reject 合成参数，Navigate/Toggle 贴近本地参考特征
- [x] 3.3 实现 Complete、Warning、Failure、Notification、Connect、Disconnect、PowerOn、PowerOff 合成参数且任意时刻不超过四声重叠
- [x] 3.4 更新 cue dump/上下文 demo，使无参考 WAV 的干净 checkout 可导出完整 palette

## 4. 文档与验证

- [x] 4.1 更新声音设计、资产边界、架构、性能与验证文档，明确参考未嵌入和真机待验收
- [x] 4.2 运行 OpenSpec strict、音频定向测试、完整 host/desktop、SDL dummy、shared-source/格式与 firmware 构建门禁
- [x] 4.3 导出最终合成 WAV，核对 golden/特征与试听页参考，记录自动证据、用户已接受的近似边界和未完成真机项

## 5. Confirm/Back 拍板原型回归

- [x] 5.1 记录 `09-semantic-hierarchy-full` Confirm/Back 的双事件 offset、有效时长、包络峰与主频边界，并先添加能够拒绝当前错误正式输出和旧滑音的失败测试
- [x] 5.2 将 Confirm/Back 正式 C++ 合成参数移植到拍板原型边界，保持四声部、无参考 WAV 运行时依赖，并在通过校准门禁后更新 PCM golden
- [x] 5.3 验证真实 App open/长按返回经 SystemServiceHost 分别路由 Confirm/Back，导出上下文试听并运行 OpenSpec strict、音频定向、host、desktop 与 firmware 门禁

## 6. System Menu Surface cue

- [x] 6.1 先添加 System Menu 展开/收起分别路由 MenuOpen/MenuClose 且不再触发 Confirm/Back 的失败测试
- [x] 6.2 将用户拍板的低音区 V2 MenuOpen/MenuClose 单起音参数接入 SoundCue、palette、名称与 cue dump
- [x] 6.3 增加两项时长、方向、单起音、确定性 PCM/golden 与真实 Runtime 上下文回归，并记录拍板试听边界
- [x] 6.4 更新音频文档并运行 OpenSpec strict、音频定向、完整 host、shared-source 与可用 firmware 门禁
