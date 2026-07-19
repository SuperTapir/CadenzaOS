## MODIFIED Requirements

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

#### Scenario: 多张可见卡片的应用名同时较长
- **WHEN** Launcher 当前卡片和相邻卡片名称都超过各自首选倍数的可用宽度
- **THEN** 每个 fallback 名称分别在所属 Cover 的可见文字区域执行受约束布局，且不与屏幕边缘或其他 Cover 重叠

#### Scenario: 卡片标题区域存在装饰边框
- **WHEN** Launcher 在任一可见卡片内绘制动态标题
- **THEN** 标题的可用矩形扣除卡片边框和内边距，任何缩放或 fallback 结果都不会覆盖装饰边框

#### Scenario: 在所有支持的 profile 和方向下布局同一菜单
- **WHEN** 相同 Launcher 数据分别在 320×170 与 400×240 profile 的 Vertical 与 Horizontal 方向下布局
- **THEN** 每种组合都根据自身区域选择确定性结果，且所有动态文本均保持在所属卡片与轨道 viewport 内
