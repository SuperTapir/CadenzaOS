## Why

Activation Timer 当前仍暴露 Clock 命名，主时间字形由通用 6×10 字体整数放大而产生明显锯齿，启动、暂停、恢复和到期缺少足够清晰的视觉反馈；同时 warped Menu 的遮罩固定在左半屏，导致面板变形期间遮罩既铺不满也没有自然渐隐。需要一次性收敛 Timer 的产品身份、视觉层级和系统过渡质量。

## What Changes

- **BREAKING**：将产品和 C++ 接口从 Clock 完整重命名为 Timer，删除 `ClockApp`、`kClockAppId` 及所有兼容别名；`kTimerAppId` 继续使用数值 `0x0101`。
- 为双显示 profile 提供 Cadenza 自有的原生 1-bit 工业仪表数字 atlas，以等宽 `MM:SS` 和光学校正冒号替代运行时放大的通用字体。
- 为 Start、Pause、Resume 增加局部、无堆分配的状态反馈；Start/Resume 使用正向扫光，Pause 使用同构的逆向扫光，并为 Reduced Motion 提供无位移高对比替代。
- 保留以大号 `TIME UP` 为核心的 Timer 到期卡，为其增加由 coordinator 持有的克制 1.2 秒导轨相位，并把完成音更新为三次清脆铃击、末次落在明亮和弦；输入抢占、冻结和确认语义不变，重复提醒由 5 秒收紧为约 2 秒。
- 修正 warped Menu 合成顺序：全屏遮罩随 reveal 渐显/渐隐，逐 scanline 面板再覆盖并侵占遮罩，完全展开像素保持不变。
- 重新设计精确标题为 `TIMER` 的 2D/3D 工业微海报封面：平面标题遮挡后方倾斜的立体倒计时盘；候选和实尺寸审阅图必须经用户明确批准后，才替换正式资产与视觉 golden。
- 将测试场景、诊断标签、snapshot 名称、文档和生成资产同步切换到 Timer 命名。

## Capabilities

### New Capabilities

- `activation-timer-presentation`: Timer 的统一命名、双 profile 时间字形、状态反馈和离开/返回清理行为。
- `timer-alert-presentation`: Timer 到期 Alert 的视觉相位、Motion Profile 表现及既有交互/音频约束。
- `system-menu-mask-transition`: warped Menu 的全屏遮罩 reveal、面板侵占和开关对称性。
- `timer-launcher-cover`: TIMER 封面的身份、尺寸、调色板、静态 handoff 和审批门禁。

### Modified Capabilities

- `runtime-verification`: 增加 Timer 重命名、双 profile 几何、反馈关键帧、Menu mask 和 Cover 端点验证要求。

## Impact

- 影响 `cadenza_apps`、`cadenza_core`、headless/desktop host、Launcher Cover renderer、CMake 资产生成和相关测试。
- C++ 源码使用方必须迁移到 `TimerApp` 与 `kTimerAppId`；运行时 catalog ID 数值与持久身份不变。
- 不增加 Timer snapshot 字段、第三张 framebuffer、heap、运行时字体解析、callback 或新的 Stop/Reset 语义；Pillow 仅参与可复现的离线视觉资产生成。
