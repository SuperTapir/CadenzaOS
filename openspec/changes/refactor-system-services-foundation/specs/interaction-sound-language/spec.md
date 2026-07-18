## MODIFIED Requirements

### Requirement: 语义声音词汇
系统 SHALL 至少定义 Navigate、Boundary、Confirm、Back、ToggleOn、ToggleOff 与 Reject 提示，并 SHALL 让 App 通过类型化系统命令请求交互语义而不是访问 audio service、平台设备操作或资源路径；system sound service SHALL 根据命令和权威快照决定接受、抑制或拒绝。

#### Scenario: Launcher 选择改变
- **WHEN** 旋钮输入实际改变 Launcher 当前选择
- **THEN** App 提交一次 Navigate 命令，系统接受后触发提示，并保留同步的视觉选择反馈

#### Scenario: 打开与返回
- **WHEN** 用户确认打开 App 或长按返回 Home App
- **THEN** 系统分别处理 Confirm 或 Back 命令，且提示与转场开始属于同一次语义动作

#### Scenario: 无效动作
- **WHEN** 用户动作没有改变状态、命令被边界拒绝或 sound service 不可用
- **THEN** 系统不触发成功提示，并可触发 Boundary/Reject 或记录明确拒绝，视觉反馈仍可用

### Requirement: 音量、静音与视觉等价反馈
system settings/sound service SHALL 维护 Muted、Low、Medium、High 四档权威会话音量且默认 Medium，并 SHALL 通过 `SystemSnapshot` 向 App 发布；App SHALL 通过命令请求改变音量。进入 Muted SHALL 立即停止所有 voice，此后渲染全零。USB microphone streaming active 时系统 SHALL 默认抑制扬声器 cue。任何重要状态 SHALL 具有声音之外的视觉反馈。

#### Scenario: Settings 切换音量
- **WHEN** 用户在 Settings 的 Sound 行确认且音量命令被接受
- **THEN** 权威音量循环到下一档，同一帧屏幕文字显示 commit 后的新档位，并以新设置决定是否播放反馈

#### Scenario: 立即静音
- **WHEN** 活动声音期间权威音量被设置为 Muted
- **THEN** 活动 voice 被清除且下一批样本全部为零

#### Scenario: USB 语音输入期间交互
- **WHEN** USB microphone stream active 且发生 Navigate、Confirm、Back 或错误动作
- **THEN** 系统保留视觉反馈并默认不向扬声器输出 cue，避免提示音被麦克风回录

#### Scenario: 无声使用
- **WHEN** 系统处于 Muted、USB 抑制或没有输出设备
- **THEN** 选择、确认、返回和错误仍通过现有或新增视觉状态表达
