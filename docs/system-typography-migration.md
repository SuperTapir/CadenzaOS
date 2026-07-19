# 系统字体迁移表

本文把 CadenzaOS 的系统界面文字映射到统一的 Roobert 语义角色。App 不接收、也不判断 `FramebufferProfile`；`MonoCanvas` 在 setup 时只按 viewport 一次性解析角色值，之后 measurement、layout 与 draw 复用缓存。语义在所有设备一致，原生字号可随 viewport density 改变。所有 App 底部操作栏使用独立 `Footer`，不再复用较大的 `Caption`。

| 角色 | 常规 viewport（400×240） | 紧凑 viewport（320×170） | 用途 |
| --- | --- | --- | --- |
| `Hero` | Roobert 24 Medium，36 px 行框 | Roobert 24 Medium，36 px 行框 | 主告警、不可缩小的核心信息 |
| `Title` | Roobert 24 Medium，36 px 行框 | Roobert 20 Medium，32 px 行框 | 页面主标题、模态主信息 |
| `Body` | Roobert 20 Medium，32 px 行框 | Roobert 20 Medium，32 px 行框 | 主要状态、选中结果、较宽松的应用标题 |
| `Menu` | Roobert 11 Bold，22 px 行框 | Roobert 11 Bold，22 px 行框 | System Menu 与 Settings 可操作行 |
| `Compact` | Roobert 11 Bold，22 px 行框 | Roobert 10 Bold，14 px 行框 | 菜单行、紧凑标题栏、按钮、重要状态 |
| `Caption` | Roobert 11 Medium，22 px 行框 | Roobert 11 Medium，22 px 行框 | 次要信息、页码、操作提示、角标 |
| `Footer` | Roobert 9 Mono Condensed，14 px 行框 | Roobert 9 Mono Condensed，14 px 行框 | App 底部操作提示 |

当前 density 阈值等价于 `min(width / 400, height / 240) >= 0.9`。它只选择经过像素修整的原生 bitmap，不对 glyph 做小数 `rem` 重采样。

## 表面映射

| 表面 | 文本 | 角色 |
| --- | --- | --- |
| Launcher fallback cover / runtime fallback | App 名称 | `Title` |
| Clock | 页面名、状态 | `Compact` |
| Clock | 时间值 | `Title`，除非该视图定义专用 Display renderer |
| Clock | 操作提示 | `Footer` |
| Motion | 页面名 | `Body` |
| Motion | 操作提示 | `Footer` |
| Settings | 装饰标题 | `Title` |
| Settings | 设置行 | `Menu` |
| Settings | 操作提示 | `Footer` |
| Animation Gallery | 顶栏名称 | `Compact` |
| Animation Gallery | 页名、页码、状态 | `Caption` |
| Animation Gallery | 底部操作提示 | `Footer` |
| Animation Gallery | A/B、选中结果 | `Title` / `Body` |
| System Menu | 菜单行 | `Menu` |
| System transient / timer badge / MIC badge | 状态文字 | `Compact` |
| Timer alert | 主信息、说明、按钮 | `Hero` / `Caption` / `Compact` |

Timer 主倒计时数字使用专用 numeral atlas；Launcher Cover 的标题字图和动画内容也保留各自业务资产。它们不属于系统 `TextRole`，不会被 viewport resolver 替换。

Settings 的 Motion 与 Sound 行复用 System Menu 的 toggle 与 volume bars，
不再把 `FULL` / `MEDIUM` 塞进 label；其他连接与方向状态仍保留文字，避免
使用语义不匹配的图标。

所有产品调用均使用原生 `scale = 1`。缩放仍保留在底层 API 中供明确的图形化效果或测试使用，但不用于模拟另一种排版层级。
