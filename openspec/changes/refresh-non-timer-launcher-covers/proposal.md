## Why

MOTION、SETTINGS 与 ANIMATION GALLERY 的现有 Cover 把功能画成速度线、零件阵列和编号轨迹，在 350×155/280×124 的 1-bit 实尺寸下过于说明化且缺少单一记忆钩子。需要在保持 TIMER 已批准像素和静态 Cover 契约的前提下，把三张升级为同一家族但不套版的工业微海报。

## What Changes

- 分别以空心圆牵引字标、共同触觉控制台、三姿态表演建立三张独立身份。
- 候选经纯黑白、双 profile、reflective preview 与 4× 轮廓审阅并由用户明确批准后，使用 contour edge-cleanup 去除 Bayer 毛边，才替换正式 source/PBM/header/golden。
- 复跑资产来源、静态交互、handoff、snapshot、构建和固件体积门禁。

## Capabilities

### New Capabilities

- `launcher-cover-art`: 定义三张非 Timer Cover 的视觉身份、简化预算、审批与双 profile 交付。

### Modified Capabilities

- `runtime-verification`: 增加新版 Cover 的来源、端点、snapshot 与固件验证。

## Impact

影响 `assets/launcher-covers/`、对应 generated headers、视觉 snapshots 和资产文档；不改变 App API、运行时资源模型、App UI 或 TIMER Cover。
