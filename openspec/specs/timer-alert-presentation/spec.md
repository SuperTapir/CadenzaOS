# Timer Alert Presentation Specification

## Purpose
定义 Timer 完成后的视觉告警、确认路径、后台提示、声音协作以及 Reduced Motion 呈现边界。

## Requirements

### Requirement: 后台 Timer 使用持久系统指示
Running 或 Paused Timer 的 owner App 不在前台时，系统 SHALL 在 persistent indicator layer 显示紧凑状态和向上取整的剩余分钟；indicator SHALL 不抢占输入，owner App 前台 SHALL 不重复显示。

#### Scenario: Launcher 显示后台 Timer
- **WHEN** Timer 启动 10 分钟 Timer 后返回 Launcher 且 remaining 为 07:18
- **THEN** Launcher 保持可操作并显示可读的 Running indicator `T 08`

#### Scenario: 最大分钟数保持在 indicator 内
- **WHEN** 后台 Running 或 Paused Timer 显示 `T 99` 或 `P 99`
- **THEN** indicator SHALL 按当前 profile 的 Compact 字体测量宽度，完整包住标签和右侧 padding，不得溢出胶囊

### Requirement: 到期使用 critical Timer Alert
Timer 进入 Expired SHALL 打开 system-owned TimerAlert，覆盖任意 App 或 System Menu、冻结 App update/input/render 并保持 system services 推进；确认 SHALL 停止 alert 并恢复原 current AppId，不产生 App exit/enter。

#### Scenario: 其他 App 中到期
- **WHEN** Motion App active 时 Timer 到期
- **THEN** TimerAlert 覆盖 Motion、current AppId 不变、Motion 不再 update，确认后恢复同一 Motion 实例

#### Scenario: System Menu 中到期
- **WHEN** System Menu active 时 Timer 到期
- **THEN** TimerAlert 以 critical 优先级替换 Menu、复用冻结背景且确认后返回被覆盖 App而不是执行隐藏的 Menu action

### Requirement: Alert 捕获完整按钮 sequence
TimerAlert SHALL 在出现时接管后续 input，并 SHALL 在已有按钮 sequence 未 release 时保持 unarmed；触发前的 release/click 不得确认 alert或泄漏到 App。

#### Scenario: 按住按钮时到期
- **WHEN** 用户按住按钮且 Timer 在该 sequence 中到期
- **THEN** alert 出现但第一次 release 只完成 capture，用户下一次完整短按才确认

### Requirement: Alert 与 transition 组合确定
TimerAlert SHALL 在 App transition 期间立即拥有输入并在当前可组合 frame 上显示，不得等待目标 App update 或丢失 expiration generation。

#### Scenario: launch transition 中到期
- **WHEN** Timer 到 Motion 的 transition 中 Timer 到期
- **THEN** alert 在该帧后成为唯一 interactive surface、App transition 不接收确认输入且确认后 runtime 回到确定的 stable App state

### Requirement: Alert 双 profile 可读且无声等价
TimerAlert SHALL 在 320×170 和 400×240 显示完成状态、configured duration 和确认提示，并 SHALL 在 Muted/Reduced Motion 下保持完整意义、bounds 安全和稳定 snapshot。

#### Scenario: 静音到期
- **WHEN** sound volume 为 Muted 且 Timer 到期
- **THEN** 不输出 PCM，但高对比 critical surface 持续到短按确认且所有文字在 framebuffer 内

### Requirement: Timer Alert owns a deterministic visual phase
SystemSurfaceCoordinator SHALL 在 Timer 首次进入 Expired 时将 Alert presentation elapsed 清零，在 Alert 活跃期间推进 elapsed，并在确认或离开 Expired 后清除。该 elapsed SHALL 为只读 presentation 信息且不得写入 Timer snapshot。

#### Scenario: 到期 edge 启动反馈
- **WHEN** Timer 状态从非 Expired 进入 Expired
- **THEN** Alert 获得输入和 App 冻结权，presentation elapsed 从零开始推进

#### Scenario: 确认到期
- **WHEN** 用户确认 Alert 或 Timer 不再 Expired
- **THEN** Alert 退出且 presentation elapsed 被清零

### Requirement: Timer Alert adapts to Motion Profile
`renderTimerAlert` SHALL 接收 visual elapsed 与 MotionProfile，并保留大号 `TIME UP` 作为到期告警主视觉。Normal SHALL 使用约 1.2 秒循环的两侧短导轨，不进行全屏反相；Reduced Motion SHALL 使用与 elapsed 无关的静态双框高强调。

#### Scenario: Normal 周期循环
- **WHEN** 以相差 1.2 秒的 elapsed 绘制同一 Timer snapshot
- **THEN** framebuffer 像素完全相同，且周期内代表相位存在可见差异

#### Scenario: Reduced Motion 静态替代
- **WHEN** 以不同 elapsed 和 Reduced Motion 绘制 Alert
- **THEN** framebuffer 像素完全相同且保持静态双框

### Requirement: Existing Timer Alert ownership and audio remain intact
视觉反馈 SHALL 保持 Alert 冻结 App、拥有输入并使用 TimerComplete 声音；TimerComplete SHALL 为三次具有快速起音与自然衰减的清脆铃击，并在末次形成明亮和弦，Alert SHALL 约每两秒重复一次且不得补播错过的轮次。Muted SHALL 仅抑制声音而不抑制视觉反馈。

#### Scenario: Muted Alert
- **WHEN** Timer 在 Muted 状态到期
- **THEN** Alert 视觉仍然活跃、输入仍被抢占且不会提交可听声音
