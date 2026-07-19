## ADDED Requirements

### Requirement: TimerComplete 是持续但有界的提醒语义
系统 SHALL 提供与 Navigate、Confirm 和 Toggle 可辨的 TimerComplete cue；Expired edge SHALL 立即请求一次并在未确认时以固定有界间隔重复，frame stall SHALL 不补播错过的历史重复，确认 SHALL 停止活动及已调度提醒声音。

#### Scenario: Timer 正常到期
- **WHEN** Running Timer 首次跨过 deadline 且音量非 Muted
- **THEN** 系统请求一次 TimerComplete，之后每个提醒间隔至多一次且不会形成无界 cue queue

#### Scenario: 到期后立即确认
- **WHEN** TimerComplete 尚在播放或含 scheduled event 时用户确认 alert
- **THEN** 所有 alert voice/scheduled event 停止且稍后不再补播

#### Scenario: 静音到期
- **WHEN** volume 为 Muted 且 Timer 到期
- **THEN** PCM 保持数字零、suppressed cue 可诊断且 TimerAlert 提供完整视觉反馈

### Requirement: TimerComplete 合成保持克制和可验证
TimerComplete SHALL 使用固定容量参数化 tone/event palette、非零 attack/release 和确定 event structure；它 SHALL 足以引起注意但不得成为持续音乐、无限 voice 或 caller 自定义 waveform。

#### Scenario: PCM golden 生成
- **WHEN** portable audio core 以固定 sample rate/volume 渲染 TimerComplete
- **THEN** duration、event offsets、peak/RMS、回零和 PCM hash 匹配批准的 golden，且 cue 结束后所有样本精确为零
