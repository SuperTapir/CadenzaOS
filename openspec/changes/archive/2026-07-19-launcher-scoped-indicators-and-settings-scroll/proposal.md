## Why

被动状态角标（后台 Timer、USB MIC）目前会盖在任意前台 App 上，干扰业务画面；同时 Settings 列表已增至 7 项且在 320×170 上无滚动，选中项会落到可视区外。需要把「仪表盘式」状态提示收敛到 Launcher，并让 Settings 在项数超过可视高度时仍能到达每一项。

## What Changes

- 为被动系统 indicator 引入显式 **presentation scope**（至少支持全局与仅 Home/Launcher），Timer 后台角标与 USB MIC 隐私角标默认改为 **仅在 Launcher（Home）上绘制**。
- **BREAKING（呈现语义）**：非 Launcher 前台 App 上不再显示后台 Timer 角标与 MIC 角标；Timer 服务、到期 TimerAlert、声音抑制与 snapshot 字段语义不变。
- Settings 列表在行总高超过可视区时滚动，保证当前选中行始终完整可见，旋钮可到达最后一项（含 ABOUT）。
- 更新相关双 profile snapshot / AppRuntime / FrameCoordinator 回归，使「只在 Launcher 可见」与「Settings 可滚到末项」成为可测契约。

## Capabilities

### New Capabilities

- `settings-list-presentation`: Settings 主列表的可视窗口、选中跟随滚动、双 profile 可达性与裁切边界。

### Modified Capabilities

- `system-overlay-presentation`: 被动/persistent indicator 增加 scope；HomeOnly 时仅在 Home/Launcher 合成。
- `timer-alert-presentation`: 后台 Timer 持久 indicator 从「非 owner 前台皆显示」改为「仅 Launcher（Home）显示」；TimerAlert critical 行为不变。
- `runtime-verification`: 后台 Timer / MIC 角标回归改为 Launcher 可见、非 Home App 不可见；Settings 增加末项可达与选中可见断言。
- `portable-runtime`: 澄清系统 snapshot/indicator 在非 Home 前台可不可见，与 Timer 服务继续运行解耦。

## Impact

- 影响 `AppRuntime` 中 Timer `statusIndicator` 合成条件、`FrameCoordinator` 中 MIC 绘制条件，以及（如引入）共享的 indicator scope 判定。
- 影响 `SettingsApp` / `renderSettingsScreen` 的布局与选中滚动状态；可能触及 Settings 相关 snapshot 基线。
- 现有用例「background Timer indicator stays legible over every built-in App」及在非 Home App 上探测 MIC 像素的测试需要改写。
- 不改变 System Menu、TimerAlert、transient toast、声音 cue 抑制策略（USB mic 仍可抑制声音），也不引入通用 modal stack 或 App 可注入的 overlay API。
