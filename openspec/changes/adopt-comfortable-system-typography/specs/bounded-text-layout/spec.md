## MODIFIED Requirements

### Requirement: 文本布局必须受显式区域约束
系统 SHALL 要求受约束文本的调用方提供非空半开矩形、显式 typography role、首选与最小整数倍数、对齐方式和溢出策略。成功的布局 SHALL 报告实际 role、scale 与绘制边界，并 SHALL 保证所有光栅化像素都位于请求矩形和 framebuffer 边界以内。布局与绘制 SHALL 使用当前 Canvas 在 setup 时为同一个 role 缓存的真实 metrics。

#### Scenario: 文本以首选倍数完整容纳
- **WHEN** 文本按请求 role 和首选倍数测量后能够放入请求矩形
- **THEN** 布局保留 role、首选倍数和请求的对齐方式，且绘制不会改变矩形外的任何像素

#### Scenario: 受约束文本请求无效
- **WHEN** 请求矩形为空、文本为空指针、role 无效、任一倍数为零，或最小倍数大于首选倍数
- **THEN** 布局返回明确的无效结果、不绘制任何像素，并在存在 diagnostic sink 时发出分类诊断
