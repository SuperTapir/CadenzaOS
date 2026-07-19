## ADDED Requirements

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
