## Why

当前系统所有文本都固定使用 6×10 terminal bitmap font，并依赖整数倍 nearest-neighbor 放大；它能保证确定性，却在 320×170 与 400×240 的高信息密度 UI 中表现得偏细、偏硬，无法形成舒适且一致的标题、正文和辅助信息层级。Playdate 的 Roobert 原生 1-bit 字号样张已经证明，逐字号修整的均匀笔画可以显著改善可读性，因此现在应把字体从单一实现常量提升为稳定的系统 typography 契约。

## What Changes

- 引入经许可审核与离线转换的 Roobert bitmap font 资源，提供 `Hero`、`Title`、`Body`、`Menu`、`Compact`、`Caption`、`Footer` 七个语义角色。
- 统一 App 操作栏与 Menu Item focus：底栏使用更轻、更矮的 `Footer`，选中背景填满 item 可用宽度并使用小圆角。
- Balanced 与 Compact 作为同一字体家族内的语义层级共同使用；App 和系统 surface 只选择角色，不选择裸字体指针或设备型号。
- typography 层根据 viewport 计算集中式 density class，像媒体查询一样选择原生 bitmap 档位：常规画布使用 Roobert 24/20/11，紧凑画布使用 20/20/10/11；App 内不得出现 Sharp/T-Embed 字体 `if/else`。
- Timer 数字、Launcher Cover、动画内容等业务 Display typography 保留独立视觉资产；系统字体只统一菜单、状态、说明和普通内容，不把业务展示字体抹平成同一种字形。
- 扩展 `MonoCanvas` 的 measurement、alignment、bounded text 与 raster path，使它们对显式字体角色保持同一组确定性 metrics。
- 为字体授权、转换可复现性、flash 成本、双 profile framebuffer、全内置 App/system surface snapshot 和可检查 PNG 建立回归门禁。
- 更新所有内置 App、Launcher、system overlay/surface 使用语义文字角色，并人工审阅双 profile 的完整视觉矩阵，修正明显裁剪、拥挤、层级混乱或笔画不舒适的页面。

## Capabilities

### New Capabilities

- `system-typography`: 定义统一 Roobert 字体家族、语义角色、viewport 响应式解析、业务 Display 字体边界和 App 内不分支约束。

### Modified Capabilities

- `mono-graphics`: 从固定单字体扩展为可审计的多字号 bitmap font descriptor、metrics 与光栅能力。
- `bounded-text-layout`: bounded text 布局必须显式携带字体角色，并以该角色的真实 metrics 决定换行与溢出。
- `runtime-verification`: 双 profile snapshot 与资源审计增加系统 typography、全 surface 视觉回归和 flash 预算覆盖。

## Impact

- 影响 `cadenza_core` 的 `MonoCanvas`、text metrics、bounded text 与 system surface，及 `cadenza_apps` 中所有文字调用点。
- 构建系统将增加固定版本字体源、离线转换工具、生成资源验证与第三方署名；字体 bitmap 增加 flash 占用但不增加 framebuffer、运行时 heap 或平台专属 rasterizer。
- 既有 framebuffer snapshot hash 将发生预期变化；几何、guard byte、裁剪诊断、确定性与双 profile 覆盖仍须保持。
