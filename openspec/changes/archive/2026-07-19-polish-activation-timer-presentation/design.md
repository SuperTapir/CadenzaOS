## Context

Timer 目前沿用 `ClockApp`/`kClockAppId` 名称和 CLOCK 封面；时间体由 `u8g2_font_6x10_tf` 以 4×/5× 最近邻放大，轮廓粗糙且占屏比例不足。App 仅有声音状态反馈，Timer Alert 静态。warped Menu 虽然逐 scanline 展开 panel，但 mask 始终只绘制稳定态左半屏，所以动画中 panel 尚未覆盖的右侧既没有遮罩，也没有 reveal 渐变。

约束包括：无 heap、无第三张 framebuffer、无动态字体、双 profile、30 FPS、Reduced Motion、Catalog ID 数值不变，以及视觉 baseline 必须由用户明确批准。

## Goals / Non-Goals

**Goals:**

- 让 Timer 成为产品、源码、测试和资产中的唯一现行名称。
- 以原生 1-bit atlas 获得大而清晰的等宽时间体，并提供可审阅母稿。
- 在不改变 Timer 服务真相的前提下增加状态与到期 presentation。
- 让 Menu mask 从整屏自然渐现，再被 panel 覆盖；关闭完全反向。
- 产出易识别、符合 Cadenza Launcher Cover 规范的 TIMER 候选并经过审批门禁。

**Non-Goals:**

- 不新增 Stop、Reset 或 Timer 持久化字段。
- 不改变 `0x0101` 的 catalog 身份、Timer 服务状态机、输入抢占和声音策略。
- 不引入字体运行时、堆分配、callback 或额外 framebuffer。
- 未获用户批准时不接受数字或封面的 visual golden。

## Decisions

### 1. 直接删除旧 Clock 符号

采用 `TimerApp` 与 `kTimerAppId`，后者继续取值 `0x0101`。不提供 alias，使遗漏迁移在全仓搜索和编译时直接暴露。相比保留兼容名，这能避免未来真正 Clock App 与 Timer 身份再次纠缠；代价是本仓源码必须原子迁移。

### 2. 使用双尺寸自有 bitmap atlas

数字只包含 `0–9` 与冒号，按平滑矢量母稿生成 320×170 与 400×240 的原生 1-bit atlas。运行时按 glyph source rect 逐字 Copy，不做缩放。经实尺寸审阅后，组合宽高锁定为 228×84 与 308×120，数字 cell 等宽，冒号使用独立窄 cell 和两个倒角方点。

不继续放大通用字体，因为最近邻只能放大原有采样误差；也不引入动态字体，因为增加内存、依赖和运行时不确定性。自有几何字形的生成器、母稿和 PBM 均可审计、可复现。

### 3. App 反馈属于纯 presentation 状态

`TimerApp` 持有 `None/Starting/Pausing/Resuming` 与 elapsed。命令成功提交后触发效果，服务 snapshot 仍是唯一业务真相。Normal 的 Start/Resume 使用从左到右的局部激活扫光，Pause 复用同一视觉语法并从右向左逆向扫过，避免额外的制动导轨或暂停符号干扰时间体。Reduced Motion 使用短暂静态双边框。`onEnter`/`onExit` 清零，避免旧动画跨 handoff 泄漏。

### 4. Alert 相位由 SystemSurfaceCoordinator 持有

Coordinator 在 Expired edge 清零 elapsed，Alert 活跃时推进，在确认或状态离开后清除；renderer 接受 elapsed 与 MotionProfile 并保持纯函数。根据实尺寸审阅反馈，告警卡保留大号 `TIME UP`、完成分钟数和反白确认按钮，不以 `00:00` 替代它。Normal 以 1.2 秒周期只驱动两侧短导轨，避免全屏反相；Reduced Motion 保持静态双框。相位不写入 `SystemSnapshot`，因为它不是服务状态，也无需跨生命周期恢复。

完成音使用三个具有快速起音和指数衰减的共振铃击，第三次叠加明亮 C 大三和弦。它继续复用 `TimerComplete` cue，将重复周期从五秒收紧到约两秒，使约 0.7 秒的单轮提示后只保留约 1.3 秒空白；不增加 PCM 资产或运行时分配。

### 5. Menu 先铺全屏 mask，再合成 panel

warped Menu 先使用 eased reveal 在整个 framebuffer 应用确定性 dither mask；随后逐 scanline Copy panel，使 panel 覆盖其到达区域。进度 0 时 mask 覆盖为 0，进度 1 时右半屏由稳定 panel 全量覆盖，因此最终像素与现有 fully-open 菜单相同。Closing 使用相同 progress 的反向轨迹，不引入额外缓冲。

### 6. Cover 使用审批式资产管线

封面采用用户批准的 2D/3D 组合：精确标题 TIMER 位于前景，完整的 `R` 遮挡后方带倾角的立体倒计时盘；盘面缺失扇区表达正在被消耗的时间，灰色面、侧壁与轮缘通过有序抖动保留体积。保留 1400×620 平滑母稿，检查硬阈值、350×155、280×124、reflective palette 与移动 track；批准后生成 PBM/header、接入 renderer/CMake、更新 golden 并删除 Clock 资产。

## Risks / Trade-offs

- [自有字形可能过于接近七段数码管或在小屏粘连] → 同时审阅 Ready/Running/Paused 的双 profile 实尺寸图，审批前不更新 golden。
- [全屏 dither 在动画早期过于抢眼] → coverage 由 eased reveal 驱动，使用现有稳定 mask 密度作为终点，并锁定中间帧测试。
- [presentation elapsed 与服务 edge 不同步] → 只在 coordinator 检测到 Expired edge 时启动，并在所有离开 Alert 的路径清零。
- [直接删除 Clock 符号造成漏改] → 全仓搜索、strict compile 和测试名称门禁共同发现遗漏；历史研究文字仅允许明确标注为旧实现。
- [Cover 视觉方向不符合预期] → 候选与实际尺寸审阅图先交用户批准，正式资产保持可回退。

## Migration Plan

1. 建立失败测试与全仓命名审计。
2. 原子迁移 C++ 类型、ID、成员、测试场景和文档名称。
3. 生成并接入数字 atlas，加入 App/Alert/Menu presentation 与几何/关键帧测试。
4. 输出数字状态预览和 Cover 候选；等待用户明确批准。
5. 批准后生成正式 Cover 资产，更新 visual golden，并运行完整 host、sanitizer、SDL、PlatformIO、allocation/size 与 diff 检查。

回退时可整体回退该 change；`0x0101` 未改变，因此没有 catalog 数据迁移。

## Open Questions

- TIMER Cover 方向及正式替换已获用户明确批准。Inter 系统字体试验因真实页面粗细不均被用户明确否决并已完全回退，不属于本 change 的交付范围；Timer 数字与告警卡的最终 visual golden 仍需以最新实尺寸预览确认。
