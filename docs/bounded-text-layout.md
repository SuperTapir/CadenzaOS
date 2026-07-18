# 受约束文本布局

动态菜单文本不能只提供锚点和字号；调用方必须同时声明视觉容器与无法完整容纳时的降级策略。`MonoCanvas::boundedText` 以固定容量完成测量、布局和绘制，不使用动态内存。

## API 契约

`BoundedTextRequest` 包含：

- `value`：以 `\0` 结尾的 ASCII 文本；
- `bounds`：完全位于当前 canvas clip 内的非空半开矩形；
- `preferredScale` / `minimumScale`：允许的整数倍数范围；
- `align`：文本块在矩形内的对齐方式；
- `overflow`：完整文本在最小倍数下仍无法容纳时的策略；
- `maximumLines`：`wrap` 可使用的最大行数；
- `phase`：`marquee` 使用的显式归一化进度，不读取墙上时钟。

`layoutText` 只生成 `BoundedTextResult`，`drawBoundedText` 绘制已解析结果，`boundedText` 依次执行两者。result 自己拥有待绘制的文本片段，最多 4 行、每行 96 个 ASCII 字节，整体保持在 512 bytes 以内；超出容量的内容必须通过 overflow policy 降级，不能触发 heap 增长。

## Policy 选择

- 所有请求都先从 `preferredScale` 向 `minimumScale` 执行 `fit`，选择能够完整容纳文本的最大倍数。
- `Ellipsis`：适合单行菜单标签；在最小倍数下保留最长前缀并追加 `...`。
- `Wrap`：适合允许多行的说明；优先在空格处分行，必要时硬拆 token，超过行数时省略最后一行。
- `Marquee`：适合明确接受运动文本的界面；偏移只由 request 的 `phase` 决定。静态菜单默认不使用。

如果区域连 `...` 或一行字体高度都无法容纳，结果为 `NoFit` 且不绘制像素。无效 request 产生 `InvalidGeometry`；有效 overflow policy 造成的缩放、截断、换行或 marquee viewport 裁剪不会产生 `ClippedGeometry` / `FullyClipped`。底层 `MonoCanvas::text` 的原有误用诊断保持不变。

## Launcher 区域

- 主标题：使用扣除双层卡片边框、最小黑色背景和 4px 文字 padding 后的内部矩形；倍数范围为 4 到 1，fallback 为 `Ellipsis`。
- Footer：左右各保留 12px 屏幕边距，中间固定保留 64px 导航区域；前后应用名只能在各自左右区域内以 1 倍 `fit`/`Ellipsis` 绘制。
- 长名称不得擦除卡片边框、进入中央圆点区域、越过屏幕边缘或产生 `text clipped`。

## 动态文本调用盘点

| 调用 | 数据上限 | 处理 |
| --- | --- | --- |
| Launcher 当前应用名 | 来自可注册 `App::name()`，长度不受核心控制 | 已迁移到主标题受约束区域 |
| Launcher 前后应用名 | 来自可注册 `App::name()`，长度不受核心控制 | 已迁移到左右独立 footer 区域 |
| Clock 时间值 | 固定 `%02d:%02d` 格式与本地 16-byte buffer | 保留现有绘制，由应用快照覆盖 |
| Settings 行文本 | 两组编译期常量 | 保留现有绘制，由双 profile 应用测试覆盖 |
| Gallery page name / page label | 固定容量枚举表与 `%02u/%02u` 格式 | 保留现有绘制，由 Gallery 快照覆盖 |
| Gallery animation state | 固定状态机名称集合 | 保留现有绘制，由 Gallery 测试覆盖 |

盘点结果中只有 Launcher 的 `App::name()` 是可扩展且无长度上限的菜单输入，因此本次不机械迁移静态标签。
