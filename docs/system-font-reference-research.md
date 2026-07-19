# 系统字体参考调研与采用决策

## 结论

CadenzaOS 采用 Playdate SDK 3.0.6 Resources 中的 Roobert 原生 1-bit bitmap，建立统一的 `Hero`、`Title`、`Body`、`Menu`、`Compact`、`Caption`、`Footer` 语义角色。Sharp 400×240 与 T-Embed 320×170 共用相同语义；`MonoCanvas` 在 setup 时按 viewport density 集中解析原生字号，App 不出现设备字体分支。Timer 数字等业务 Display typography 保持独立。

本结论来自对官方规范、锁定资源、许可证、实际 glyph image table 和 Cadenza 1-bit 样张的联合验证，不是仅依据 Playdate 截图或二手介绍。

## 锁定参考与复用边界

| 参考 | 版本 / commit | 许可证 | 阅读与验证范围 | 决策 |
| --- | --- | --- | --- | --- |
| [Playdate SDK](https://play.date/dev/) | Linux SDK 3.0.6，官方 latest 下载入口在 2026-07-19 的实际重定向版本 | SDK fonts 单独为 [CC BY 4.0](https://sdk.play.date/2.7.6/Inside%20Playdate.html) | SDK `README.md` legal section、Roobert `.fnt` metrics/width/kerning、六张透明 1-bit image table | 采用六套 Roobert font source；不采用 SDK runtime、compiler 或其他代码 |
| [Designing for Playdate](https://help.play.date/developer/designing-for-playdate/) | 2026-07-19 在线版本 | 文档参考 | 系统 UI 使用 Roobert 20 Medium、标题 24 Medium；正文 cap height 与至少 2 px 笔画建议 | 采用其阅读尺寸原则，但 Cadenza 自己定义 component/layout |
| [Inter](https://github.com/rsms/inter/tree/e3a3d4c57d5ecc01453a575621882a384c1995a3) | 4.1 / `e3a3d4c57d5ecc01453a575621882a384c1995a3` | OFL-1.1 | Medium TTF 在 12/14/20/24 px、关闭灰阶抗锯齿后的 1-bit 栅格 | 不采用：细横画与曲线在纯 1-bit 下比 Roobert 原生 bitmap 更碎 |
| [Atkinson Hyperlegible](https://github.com/googlefonts/atkinson-hyperlegible/tree/1cb311624b2ddf88e9e37873999d165a8cd28b46) | `1cb311624b2ddf88e9e37873999d165a8cd28b46` | OFL-1.1 | Regular TTF 在同一 Cadenza settings 几何和纯 1-bit 阈值下的可读性 | 不采用：字符区分优秀，但低字号笔画密度不如 Roobert 均匀 |
| Asheville Sans | Playdate SDK 3.0.6 | CC BY 4.0 | 14 Light/Bold 与 24 Light 原生 image table | 不采用为系统默认：官方本身将 Asheville 定位为 font load failure fallback，且样张明显偏细 |

## 当前问题的根因

当前 `MonoCanvas` 在构造时固定 `u8g2_font_6x10_tf`。所有文字层级只能对同一 6×10 bitmap 做整数 nearest-neighbor 放大，因此：

- 1× 的正文在高信息密度 surface 上偏小偏硬；
- 2×/3× 会同步放大原始锯齿和方形转角，不会产生更自然的 20/24 px 曲线；
- App 只能用 scale 表达“大小”，不能表达标题、正文、紧凑正文和辅助信息的稳定语义；
- measurement 也绑定同一字体，直接替换默认 font 会同时改变所有 bounds，必须做完整视觉矩阵而不能只看单个页面。

Roobert 的优势不是灰阶抗锯齿，而是 11、20、24 等原生 1-bit 字号分别经过像素修整。关闭所有灰阶后，它仍比从 TTF 临时阈值化的候选保持更连续的曲线与更均匀的 stem。

## 样张与最小实验

同一 settings-like geometry 在纯 0/255 framebuffer 下比较了：Roobert balanced、Roobert compact、Inter Medium、Atkinson Hyperlegible、Asheville Sans，以及 Roobert 320×170 版。所有候选均禁用灰阶抗锯齿；Roobert 源 glyph 直接来自锁定 image table，TTF 候选以 1-bit FreeType/Pillow raster 作为对照。

实验结论：

- Roobert balanced 在 400×240 上具有最稳定的标题/正文/辅助值层级；
- Roobert compact 适合辅助信息和高密度组件，但它仍是同一套语义系统，而不是设备主题；
- 320×170 使用相同 role，但 `Title` 与 `Compact` 分别解析为 20 Medium 与 10 Bold，减少拥挤且不把设备判断传播到 App；
- Inter/Atkinson 的桌面审美不能直接推导出 1-bit 小字号表现，必须以无灰阶 framebuffer 为准。

## 产品映射

| Role | 常规 viewport | 紧凑 viewport | 用途 |
| --- | --- | --- | --- |
| `Hero` | Roobert 24 Medium | Roobert 24 Medium | 主告警、不可缩小的核心信息 |
| `Title` | Roobert 24 Medium | Roobert 20 Medium | 页面/主要状态标题 |
| `Body` | Roobert 20 Medium | Roobert 20 Medium | 常规列表、主要操作和核心读数 |
| `Menu` | Roobert 11 Bold | Roobert 11 Bold | System Menu 与 Settings 可操作行 |
| `Compact` | Roobert 11 Bold | Roobert 10 Bold | 空间受限但仍重要的文字 |
| `Caption` | Roobert 11 Medium | Roobert 11 Medium | 辅助值、页码、状态说明 |
| `Footer` | Roobert 9 Mono Condensed | Roobert 9 Mono Condensed | App 底部操作提示 |

resolver 不接收 profile 或设备型号，只读取 immutable viewport。相同 role 的语义一致，但允许得到表中不同的原生 descriptor；相同 viewport 的结果必须完全确定。

## 资源尺寸与性能判断

按 glyph 宽度紧密 bit packing 的初步上界为：Roobert 20 Medium 约 5.8 KB、24 Medium 约 7.4 KB、完整 11 Medium 约 9.3 KB、完整 11 Bold 约 10.3 KB，另加 glyph/kerning metadata。最终 converter 只打包产品 manifest 允许的 glyph，并在生成时报告精确字节。

字体数据进入 flash/只读区，不增加 framebuffer，不需要 runtime PNG/JSON parsing、heap 或 TTF rasterizer。代价是 flash 高于当前约 2 KB 单字体；这是获得原生舒适字形的有意识取舍。

## 授权与署名

Playdate SDK `README.md` 明确写明 Playdate fonts 使用 CC BY 4.0。仓库复制的是 fonts resource，不复制 SDK runtime/tool。发布时必须保留 license 和署名、说明转换，并且不暗示获得 Panic/Playdate 背书。

## 待实体设备验证

Host 可以证明 1-bit pixel geometry、边界、层级和确定性，但不能完全证明两块实体屏幕在真实 PPI、反射、照明与观看距离下的舒适度。最终完成可以覆盖 P0–P7 与所有 host visual regression；P8 仍须在 Sharp 与 T-Embed 真机各检查一次，且不应通过增加 profile 字体分支来修复物理观感差异。
