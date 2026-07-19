## ADDED Requirements

### Requirement: 后台 Timer 使用持久系统指示
Running 或 Paused Timer 的 owner App 不在前台时，系统 SHALL 在 persistent indicator layer 显示紧凑状态和向上取整的剩余分钟；indicator SHALL 不抢占输入，owner App 前台 SHALL 不重复显示。

#### Scenario: Launcher 显示后台 Timer
- **WHEN** Timer 启动 10 分钟 Timer 后返回 Launcher 且 remaining 为 07:18
- **THEN** Launcher 保持可操作并显示可读的 Running indicator `T 08`

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
