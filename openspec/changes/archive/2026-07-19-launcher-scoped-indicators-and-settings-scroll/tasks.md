## 1. 失败测试先行

- [x] 1.1 改写后台 Timer indicator 测试：Home 可见、Settings/Motion 等非 Home 不可见；保留 Home 上 `T 99`/`P 99` 胶囊度量
- [x] 1.2 改写 USB MIC 隐私角标测试：Home + streaming 可见；非 Home + streaming 不可见；cue 抑制仍断言
- [x] 1.3 新增 Settings 双 profile 测试：连续 turn 到最后一项时选中行完整在 canvas 内，click 仍打开 About
- [x] 1.4 确认上述测试在当前实现下失败（红灯）

## 2. Indicator scope

- [x] 2.1 在 presentation/core 增加封闭 `IndicatorScope`（至少 `HomeOnly`，可保留 `Global`/`NonOwner` 扩展位）与 `shouldShowPassiveIndicator(scope, currentId, homeId)` 判定
- [x] 2.2 `AppRuntime` Timer 后台角标改为 `HomeOnly`（替换 `owner != currentId` 的全局非 owner 绘制）
- [x] 2.3 `FrameCoordinator` MIC 绘制改为同一 `HomeOnly` 判定
- [x] 2.4 跑通 1.1/1.2 相关单测并修正诊断/注释中的旧语义

## 3. Settings 选中跟随滚动

- [x] 3.1 `SettingsApp` / `renderSettingsScreen` 增加可视窗口与 `scrollOffset`（行索引优先），选中变化时 clamp 使选中行完整可见
- [x] 3.2 保持 wrap 选择与既有 click 命令语义不变；About 进出不受滚动影响
- [x] 3.3 跑通 1.3 Settings 末项可达测试

## 4. Snapshot 与回归门禁

- [x] 4.1 更新受影响的 background-timer / Settings / 相关 App snapshot 基线（双 profile），保留 failure PNG 可审计
- [x] 4.2 跑相关 unit/integration/snapshot 与既有 Timer E2E 烟雾，确认 TimerAlert/Menu/cue 抑制未回归
- [x] 4.3 必要时同步简短文档注释（若实现报告/基线表仍写「覆盖每个内置 App」）

## 5. 收尾

- [x] 5.1 `openspec validate launcher-scoped-indicators-and-settings-scroll --strict`（或等价校验）通过
- [x] 5.2 确认 tasks 全部勾选，准备 `/opsx:apply` 后的 archive 流程
