# bounded-text-layout Specification

## Purpose
定义 1-bit 画布中受显式矩形约束的文本测量、缩放、对齐与溢出策略，使动态标签在 T-Embed 和 Sharp 分辨率下保持可读且不越界。
## Requirements
### Requirement: 文本布局必须受显式区域约束

系统 SHALL 要求受约束文本的调用方提供非空半开矩形、显式 typography role、首选与最小整数倍数、对齐方式和溢出策略。成功的布局 SHALL 报告实际 role、scale 与绘制边界，并 SHALL 保证所有光栅化像素都位于请求矩形和 framebuffer 边界以内。布局与绘制 SHALL 使用当前 Canvas 在 setup 时为同一个 role 缓存的真实 metrics。

#### Scenario: 文本以首选倍数完整容纳
- **WHEN** 文本按请求 role 和首选倍数测量后能够放入请求矩形
- **THEN** 布局保留 role、首选倍数和请求的对齐方式，且绘制不会改变矩形外的任何像素

#### Scenario: 受约束文本请求无效
- **WHEN** 请求矩形为空、文本为空指针、role 无效、任一倍数为零，或最小倍数大于首选倍数
- **THEN** 布局返回明确的无效结果、不绘制任何像素，并在存在 diagnostic sink 时发出分类诊断

### Requirement: Fit 必须确定性地选择最大的可用倍数
`fit` 溢出策略 SHALL 从首选倍数向下测试到最小倍数，并 SHALL 选择能让文本宽高都完整放入请求矩形的最大倍数。布局结果 SHALL 只取决于请求参数和字体度量。

#### Scenario: 长标题缩小后可以容纳
- **WHEN** 单行标题无法以首选倍数容纳，但能以允许范围内的较小倍数容纳
- **THEN** 布局选择最大的可用倍数、保留完整标题，并报告已经执行缩放

#### Scenario: 文本在最小倍数下仍无法容纳
- **WHEN** 完整文本在所有允许倍数下都无法容纳
- **THEN** `fit` 执行请求中显式声明的 fallback policy，而不是静默裁剪文本

### Requirement: 溢出降级策略必须显式且受边界约束
系统 SHALL 支持 `ellipsis`、`wrap` 和 `marquee` fallback。`ellipsis` SHALL 保留可见的截断标记，`wrap` SHALL 遵守调用方提供的固定最大行数，`marquee` SHALL 提供由调用方显式 phase 驱动的裁剪 viewport。所有 fallback SHALL 保持在请求矩形内，并 SHALL 避免动态内存分配。

#### Scenario: Ellipsis 截断单行文本
- **WHEN** 文本在最小倍数下仍无法完整容纳，且 fallback 为 `ellipsis`
- **THEN** 系统在矩形内绘制能够容纳的最长前缀和 ASCII 省略标记 `...`，并在结果中报告发生截断

#### Scenario: Wrap 达到最大行数
- **WHEN** 换行后的内容超过请求的最大行数
- **THEN** 所有可见行都位于矩形内，且最后一条可见行使用省略号截断

#### Scenario: Marquee 确定性推进
- **WHEN** 使用相同的溢出文本、矩形、倍数和归一化 phase 重复执行布局
- **THEN** 系统不读取墙上时钟，并产生相同的偏移量和可见像素

### Requirement: 预期溢出必须与图形误用诊断区分
执行已声明的受约束文本溢出策略时，系统 SHALL NOT 仅因为内容被缩放、截断、换行或被 marquee viewport 有意裁剪，就发出 `ClippedGeometry` 或 `FullyClipped` 误用诊断。无效请求和超出已解析边界的意外写入仍 SHALL 可被诊断。

#### Scenario: Marquee 有意裁剪文本
- **WHEN** marquee 文本超出 viewport，但通过有效的受约束布局结果进行绘制
- **THEN** 像素被限制在 viewport 内，且系统不发出图形误用诊断

### Requirement: 动态应用标签必须使用受约束布局

Launcher 的 App Cover 或通用 fallback 中的每个动态名称 SHALL 声明扣除 Cover 安全内边距后的可用区域与溢出行为，而不是无条件以固定倍数绘制。受支持的 320×170 和 400×240 profile SHALL 在不发生意外 framebuffer 裁剪的情况下呈现当前与相邻 Cover；有意只露出边缘的相邻卡片 SHALL 在文字区域不足时省略 fallback 文字，而不是把文字裁到轨道 viewport 外。

#### Scenario: 在 320×170 下选中 Animation Gallery
- **WHEN** Launcher 在 T-Embed profile 下将 `Animation Gallery` 渲染为当前卡片
- **THEN** 完整标题以允许范围内的最大可用倍数显示在卡片内部，且该标题不产生 text-clipped 诊断

#### Scenario: 异常超长应用名称超过最小倍数容量
- **WHEN** 已注册应用名称在最小倍数下仍宽于其可见卡片文字区域
- **THEN** Launcher 执行已声明的 `ellipsis` fallback，且不与卡片边框、其他卡片或导航指示器重叠

#### Scenario: 相邻卡片只露出边缘
- **WHEN** 动画中一个相邻卡片的可见内区不足以容纳最小文字布局
- **THEN** Launcher 保留卡片空间预告但不绘制该名称，且不发出意外裁剪诊断

### Requirement: 菜单动态文本必须占用互不侵占的独立区域

菜单 SHALL 在绘制动态文本前，为每张可见 Cover 中需要系统 fallback 的标题分配明确且互不重叠的矩形。每个文本布局 SHALL 只使用所属 Cover 与轨道 viewport 的交集，不得依赖 framebuffer 边缘的最终裁剪来形成视觉结果。Launcher SHALL NOT 绘制与相邻 Cover 重复的 footer 名称或页码指示。

#### Scenario: 左右相邻应用名同时较长
- **WHEN** Launcher footer 的前一个和后一个应用名称都超过各自首选倍数的可用宽度
- **THEN** 两个名称分别在左右标签区域内执行受约束布局，且不与中央导航指示器、屏幕边缘或彼此重叠

#### Scenario: 主标题区域存在装饰边框
- **WHEN** Launcher 在卡片内绘制动态主标题
- **THEN** 主标题的可用矩形扣除卡片边框和内边距，任何缩放或 fallback 结果都不会覆盖装饰边框

#### Scenario: 在所有支持的 profile 下布局同一菜单
- **WHEN** 相同的 Launcher 菜单数据分别在 320×170 和 400×240 profile 下布局
- **THEN** 每个 profile 都根据自身区域选择确定性结果，且所有动态文本均保持在所属菜单区域内

#### Scenario: 多张可见卡片的应用名同时较长
- **WHEN** Launcher 当前卡片和相邻卡片名称都超过各自首选倍数的可用宽度
- **THEN** 每个 fallback 名称分别在所属 Cover 的可见文字区域执行受约束布局，且不与屏幕边缘或其他 Cover 重叠

#### Scenario: 卡片标题区域存在装饰边框
- **WHEN** Launcher 在任一可见卡片内绘制动态标题
- **THEN** 标题的可用矩形扣除卡片边框和内边距，任何缩放或 fallback 结果都不会覆盖装饰边框

#### Scenario: 在所有支持的 profile 和方向下布局同一菜单
- **WHEN** 相同 Launcher 数据分别在 320×170 与 400×240 profile 的 Vertical 与 Horizontal 方向下布局
- **THEN** 每种组合都根据自身区域选择确定性结果，且所有动态文本均保持在所属卡片与轨道 viewport 内
