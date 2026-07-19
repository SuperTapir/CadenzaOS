## MODIFIED Requirements

### Requirement: 后台 Timer 使用持久系统指示
Running 或 Paused Timer 的 owner App 不在前台时，系统 SHALL 仅在 Home/Launcher 前台于 persistent indicator layer 显示紧凑状态和向上取整的剩余分钟；indicator SHALL 使用 `HomeOnly` scope、不抢占输入，owner App 前台 SHALL 不重复显示，且非 Home 前台 App SHALL 不显示该 indicator。

#### Scenario: Launcher 显示后台 Timer
- **WHEN** Timer 启动 10 分钟 Timer 后返回 Launcher 且 remaining 为 07:18
- **THEN** Launcher 保持可操作并显示可读的 Running indicator `T 08`

#### Scenario: 非 Home App 不显示后台 Timer 角标
- **WHEN** Timer 在后台 Running 或 Paused 且当前 App 为 Settings、Motion 或其他非 Home App
- **THEN** 该 App 画面不绘制 Timer status indicator，Timer 服务继续推进

#### Scenario: 最大分钟数保持在 indicator 内
- **WHEN** Launcher 前台且后台 Running 或 Paused Timer 显示 `T 99` 或 `P 99`
- **THEN** indicator SHALL 按当前 profile 的 Compact 字体测量宽度，完整包住标签和右侧 padding，不得溢出胶囊
