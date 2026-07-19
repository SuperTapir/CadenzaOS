## ADDED Requirements

### Requirement: HomeOnly 角标与 Settings 滚动有自动化回归
自动化 SHALL 锁定 `HomeOnly` Timer/MIC 角标仅在 Home 可见、在代表性非 Home App 不可见，并 SHALL 验证 Settings 在双 profile 下可通过旋钮到达最后一项且选中行完整落在 canvas 内。

#### Scenario: Timer 角标 Home 可见非 Home 不可见
- **WHEN** 后台 Timer Running 且分别渲染 Home 与 Settings（或 Motion）画面
- **THEN** 仅 Home framebuffer 含 Timer indicator 像素，非 Home 不含

#### Scenario: MIC 角标 Home 可见非 Home 不可见
- **WHEN** `microphoneInUse` 为真且分别在 Home 与非 Home App 上跑 frame
- **THEN** 仅 Home 出现 MIC 角标，非 Home 不出现

#### Scenario: Settings 旋到末项仍可见
- **WHEN** 在 320×170 与 400×240 上将 Settings 选中项转到最后一项
- **THEN** 选中行完整位于 canvas 可视区内且可通过 click 触发该行动作（如打开 About）

## MODIFIED Requirements

### Requirement: Timer 状态机和时间边界先由失败测试锁定
自动化 SHALL 在实现前覆盖 Ready/Running/Paused/Expired、1/99 分钟选择与服务边界、非法命令、capability/owner、deadline 前/等于/之后、timestamp regression、large step、generation 和 zero-allocation。

双 profile AppRuntime 测试 SHALL 在 Home/Launcher 前台以后台 `T 99` 验证最大分钟标签及右侧 padding 均位于 System indicator 胶囊内，并 SHALL 断言非 Home 前台不绘制该 indicator。

#### Scenario: Deadline inclusive boundary
- **WHEN** host 分别推进到 deadline 前 1 ms、恰好 deadline 和 deadline 后 1 ms
- **THEN** 只有后两者为 Expired，expiration edge/generation/cue 次数与规格一致

#### Scenario: 最大标签在 Home 上可测
- **WHEN** Home 前台且后台 Timer 显示 `T 99` 或 `P 99`
- **THEN** indicator 胶囊完整包住标签与右侧 padding
