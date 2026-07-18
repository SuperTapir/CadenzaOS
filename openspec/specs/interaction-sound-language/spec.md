# interaction-sound-language Specification

## Purpose
定义 Cadenza OS 的交互声音语义、克制音色方向、高频输入抑制、会话音量、静音与视觉等价反馈契约。
## Requirements
### Requirement: 语义声音词汇
系统 SHALL 至少定义 Navigate、Boundary、Confirm、Back、ToggleOn、ToggleOff 与 Reject 提示，并 SHALL 让调用方请求交互语义而不是平台设备操作或资源路径。

#### Scenario: Launcher 选择改变
- **WHEN** 旋钮输入实际改变 Launcher 当前选择
- **THEN** 系统触发一次 Navigate，并保留同步的视觉选择反馈

#### Scenario: 打开与返回
- **WHEN** 用户确认打开 App 或长按返回 Launcher
- **THEN** 系统分别触发 Confirm 或 Back，且提示与转场开始属于同一次语义动作

#### Scenario: 无效动作
- **WHEN** 用户动作没有改变状态或被边界拒绝
- **THEN** 系统不触发成功提示，并可触发 Boundary 或 Reject

### Requirement: 克制的 Cadenza 声音语言
系统 SHALL 使用短促、带 attack/release 的参数化数字音色，以 triangle 为主体、低增益 PolyBLEP square 为数字边缘；Confirm 与 ToggleOn SHALL 具有上行动势，Back 与 ToggleOff SHALL 具有下行动势，Boundary/Reject SHALL 与成功音色可区分。

#### Scenario: 成对提示方向可辨
- **WHEN** 以相同音量依次渲染 Confirm 与 Back
- **THEN** Confirm 的结束频率高于开始频率，Back 的结束频率低于开始频率

#### Scenario: 避免直流点击
- **WHEN** 任一系统 cue 开始和结束
- **THEN** 非零 attack 或 release 使波形不会从峰值瞬时切入或切断

#### Scenario: Playdate 风格密度
- **WHEN** 日常 Navigate 与 Confirm/Back 同时出现在短交互序列
- **THEN** Navigate 保持单一短音，Confirm/Back 仅使用短方向轮廓，不形成持续音乐或奖励音墙

### Requirement: 高频输入抑制
系统 SHALL 为 Navigate 使用最短重触发间隔，并 SHALL 把单个 `InputFrame` 中任意幅度的 turn 聚合为至多一个 Navigate，不得排队补播被抑制、溢出或丢弃的历史 tick。

#### Scenario: 快速连续旋转
- **WHEN** 多次 Navigate 请求发生在冷却窗口内
- **THEN** 只有第一次请求启动 voice，其余请求被丢弃且稍后不会补播

#### Scenario: 冷却后再次旋转
- **WHEN** Navigate 冷却时间已经过去并再次改变选择
- **THEN** 新的 Navigate 被接受

### Requirement: 音量、静音与视觉等价反馈
系统 SHALL 提供 Muted、Low、Medium、High 四档会话音量且默认 Medium；进入 Muted SHALL 立即停止所有 voice，此后渲染全零。任何重要状态 SHALL 具有声音之外的视觉反馈。

#### Scenario: Settings 切换音量
- **WHEN** 用户在 Settings 的 Sound 行确认
- **THEN** 音量循环到下一档，屏幕文字立即显示新档位，并以新设置决定是否播放反馈

#### Scenario: 立即静音
- **WHEN** 活动声音期间把音量设为 Muted
- **THEN** 活动 voice 被清除且下一批样本全部为零

#### Scenario: 无声使用
- **WHEN** 系统处于 Muted
- **THEN** 选择、确认、返回和错误仍通过现有或新增视觉状态表达
