# Timer Launcher Cover Specification

## Purpose
定义 Timer Launcher Cover 的原创资产来源、双 profile 派生、1-bit 转换、边界安全与像素稳定性契约。

## Requirements

### Requirement: TIMER Cover has a singular functional identity
Launcher Cover SHALL 使用精确标题 `TIMER` 和暗色工业微海报构图。平面标题 SHALL 位于前景，完整的 `R` SHALL 遮挡后方带倾角的立体倒计时盘；盘面缺失扇区 SHALL 表达正在被消耗的时间。灰色面、侧壁与轮缘 SHALL 在 1-bit 版本中以稳定有序抖动保留体积。Cover MUST NOT 使用钟楼、`CLOCK` 标题或缩小版 App UI。

#### Scenario: Cover 候选视觉审阅
- **WHEN** 审阅 1400×620 母稿及 350×155、280×124 实尺寸版本
- **THEN** TIMER 标题可快速识别，前景 `R` 与后方立体倒计时盘的遮挡关系清晰，缩小后仍保有 2D/3D 层次且没有被禁止元素

### Requirement: TIMER Cover passes the Cadenza asset pipeline
Cover SHALL 通过纯黑白阈值、350×155、280×124、reflective palette 和移动 track 检查，并保持静态 immutable handoff。

#### Scenario: 正式资产验证
- **WHEN** 已批准 Cover 被转换、打包并用于 Timer launch/return
- **THEN** PBM/header source checks、immutability、端点 snapshot 和 30 FPS 相邻帧变化测试全部通过

### Requirement: Cover integration requires explicit approval
系统 MUST NOT 用候选替换正式 PBM、header 或 visual golden，除非用户已明确批准候选视觉。

#### Scenario: 候选尚未批准
- **WHEN** 仅完成母稿和实尺寸审阅图
- **THEN** 现有正式 Cover 和 approved golden 保持不变
