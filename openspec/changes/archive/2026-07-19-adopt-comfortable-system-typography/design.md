## Context

`MonoCanvas` 当前在构造函数中把 U8g2 context 固定为 `u8g2_font_6x10_tf`；所有 measurement、alignment、bounded text 和 draw 都隐式读取这一字体，仅能以整数 nearest-neighbor 倍数放大。结果虽然跨平台确定，但 2×/3× 只是放大 6×10 的像素结构，无法获得 Playdate 设置页中 Roobert 20/24 那种逐字号修整、笔画均匀的阅读体验。

本次调研锁定 Playdate SDK 3.0.6 的 Roobert 24 Medium、20 Medium、11 Bold、11 Medium、10 Bold 与 9 Mono Condensed 源 `.fnt` 与 image table。官方 SDK README 和开发文档声明 Playdate fonts 使用 CC BY 4.0；仓库将记录版本、单文件 SHA-256、署名和修改/转换说明。对比 spike 还锁定 Inter 4.1 与 Atkinson Hyperlegible commit `1cb311624b2ddf88e9e37873999d165a8cd28b46`，但纯 1-bit、无灰阶抗锯齿样张显示它们的细横画与曲线不如 Roobert 原生 bitmap 稳定，因此不进入产品资源。

两个 framebuffer profile 仍为 320×170 与 400×240，均使用 1-bit、无 heap 的共享 core。用户要求语义角色保持一致，但像 CSS 变量一样允许不同 viewport 在 setup 时解析为不同原生字号；设备条件不得扩散为 App 内 `if/else`。Timer 数字等业务 Display typography 也不得被系统默认字体替换。

## Goals / Non-Goals

**Goals:**

- 用 Roobert 原生 bitmap 字号形成舒适、均匀、统一的系统字体家族。
- 用 `Hero`、`Title`、`Body`、`Menu`、`Compact`、`Caption`、`Footer` 语义变量替代 App 对系统字体资源的认识，并在 Canvas setup 时由 viewport 集中解析。
- 把所有 App 底部操作栏统一为较轻、较矮的 `Footer`，并让 menu focus 背景铺满 item 可用宽度且使用小圆角。
- 保留 Timer 数字、Launcher Cover 和动画内容等业务 Display typography 的独立字形。
- 保持 measurement、alignment、bounded layout、clipping、black/white draw 与 snapshot 的确定性。
- 不增加运行时文件解析、动态内存、第二个 framebuffer 或平台专属字体引擎。
- 对所有内置 App、Launcher、system surface/overlay 的双 profile 静态画面执行可检查的视觉回归。

**Non-Goals:**

- 不提供用户可安装字体、运行时字体发现或主题包。
- 不加入 anti-aliasing、灰阶 framebuffer、矢量字体或运行时 TTF/OTF rasterizer。
- 不让 Sharp 与 T-Embed 在 App 代码中维护分散的 typography token 或设备条件。
- 不在本次增加完整 Unicode shaping、CJK 字库或 bidirectional layout；转换资源只纳入当前产品可达文本所需字符与明确 fallback。

## Decisions

### 1. 语义角色像 CSS 变量一样在 Canvas setup 时解析

App 只声明 `Hero`、`Title`、`Body`、`Menu`、`Compact`、`Caption`、`Footer`。`MonoCanvas` 构造时以 `density = min(width / 400, height / 240)` 计算一次 viewport density，并把七个语义变量解析为原生 bitmap descriptor 后保存在 Canvas。常规档为 `24 Medium / 24 Medium / 20 Medium / 11 Bold / 11 Bold / 11 Medium / 9 Mono Condensed`，紧凑档为 `24 Medium / 20 Medium / 20 Medium / 11 Bold / 10 Bold / 11 Medium / 9 Mono Condensed`。320×170 落入紧凑档，400×240 落入常规档；measurement、layout 和 draw 共享同一份解析结果。`Hero` 保持 24 Medium，供 Timer 主告警等不能随紧凑页面标题一起缩小的核心信息使用；`Menu` 保持 11 Bold，使导航项不随紧凑标签一起缩到 10 Bold。

未采用任意 `rem` 小数倍缩放：1-bit bitmap 重采样会造成笔画粗细和孔洞不稳定。未采用 App 内按 profile 映射：它会把设备条件传播到业务组件。resolver 只读取 viewport，不读取 `FramebufferProfile` 或设备型号；不同设备的变量值可以不同，但组件语义和调用代码一致。

### 2. 系统 typography 与业务 Display typography 分层

Roobert 语义变量负责系统菜单、状态、说明、普通标题与正文。Timer 专用数字 atlas、Launcher Cover 字图、动画或未来 App 专用 display face 继续通过明确的业务 renderer 绘制，不强行映射到 `TextRole`。独立 Display 资产仍接受许可证、确定性、clip 和 snapshot 审计，但不受系统 typography resolver 替换。

### 3. `MonoCanvas` 接收显式 `TextRole`，bounded request 保存同一角色

`measureText`、`text`、`BoundedTextRequest` 和 `BoundedTextResult` 都显式携带 `TextRole`；draw 必须复用 layout 结果中的 role。保留整数 scale 仅用于确有需要的像素放大兼容场景，但系统 typography 的七个角色均使用原生 scale 1。迁移完成后，产品调用点不得通过 scale 模拟另一个系统字号。

为避免一次性破坏所有调用方，可先让新参数具有 `Caption` 默认值并立即逐调用点迁移；验证任务最终使用静态搜索门禁禁止产品代码依赖隐式默认值或 profile font branch。

### 4. 使用简单、平台无关的只读 bitmap descriptor，而非继续生成 U8g2 私有字体格式

离线工具把 Playdate `.fnt` 和透明 PNG image table 转成：font metrics、ASCII/special glyph metadata、按 glyph 紧密打包的 MSB-first 1-bit rows、tracking 和非零 kerning pair。运行时 descriptor 仅包含只读 span/pointer 和整数，不解析 PNG/JSON，不分配 heap。`MonoCanvas` 直接把 glyph bits 写入既有 framebuffer，并遵守 clip 与 draw color。

未采用运行时读取 Playdate `.pft/.fnt`：固件没有资源文件系统契约，且会引入 parser、错误处理与 RAM。未采用 TTF/FreeType：成本与 1-bit 手工 hint 目标不匹配。未采用把资源转换成 U8g2 encoded array：该格式的生成器/压缩语义会把新的公共字体管线绑定到 U8g2 私有编码；项目主 spec 已要求平台无关 bitmap descriptor。

### 5. 源资源、生成资源与授权都进入可复现门禁

仓库保存六套最小字体源、CC BY 4.0 license、Panic/Playdate 字体署名、SDK 3.0.6 pin 和 SHA-256。Python/Pillow 离线转换器必须以固定阈值读取 alpha/black pixel，选择显式 glyph 集，输出稳定 C++ 数据和 manifest；CMake `--check` target 在源变更或生成结果 stale 时失败。Pillow 只用于构建时转换，不进入 firmware/runtime dependency。

### 6. 视觉验收既用 hash 防漂移，也用完整 PNG contact sheet 防“稳定地难看”

单测覆盖 glyph、kerning、metrics、alignment、clip、白字反绘、bounded layout、setup-time density 解析与设备隔离。既有 App snapshot hash 在迁移后整体更新，并保留 failure PNG。额外提供确定性工具，一次导出两种 viewport 下 Launcher、Clock、Motion、Settings、Gallery、system menu/overlay、业务 Display typography 与代表性长文本页面，生成 contact sheet；人工审阅记录必须逐项确认无意外裁剪、重叠、过细笔画、密度失衡、业务字体误替换和层级混乱。

## Risks / Trade-offs

- [六套原生字号增加 flash，明显高于当前单一约 2 KB 字体] → 转换器只打包产品 glyph 集和非零 kerning；构建报告每套与总字节，并设置显式预算，绝不以运行时缩放换回难看的字形。
- [24/11 字号会让 320×170 的既有固定布局显得拥挤] → 紧凑 viewport 在 setup 时把 `Title`/`Compact` 解析为 20 Medium/10 Bold；仍溢出的内容再通过 bounds test 调整布局、滚动或 visible content，禁止 App 按设备分支。
- [viewport 阈值可能让未知尺寸选择不合适的档位] → resolver 的公式与阈值进入单测，并只返回经过 hint 的原生 bitmap 档位；未来新增屏幕先以 visual matrix 和真机复核阈值，不做任意小数缩放。
- [直接迁移所有 `text()` 调用可能误选角色] → 按 surface 分类迁移并生成全矩阵 PNG；role 选择由视觉语义而非旧 scale 机械映射决定。
- [Playdate font 文件含当前不需要的扩展 glyph] → manifest 明确打包集合与 fallback；若未来增加本地化，另立 change 评估字库、输入和内存。
- [CC BY 4.0 attribution 遗漏] → license、third-party notices、visual asset manifest 和生成文件头四处形成可审计链路。
- [自研 bitmap renderer 与既有 U8g2 raster 行为不一致] → 先写 byte-level golden、guard-byte 与 alignment case；U8g2 仍服务既有 primitives，字体 descriptor 只替换 text path。

## Migration Plan

1. 纳入锁定字体源、许可与研究记录，建立转换器和 stale/size test。
2. 先增加 bitmap descriptor、setup-time viewport resolver、role mapping 和失败的 metrics/raster tests，再实现新 text path。
3. 扩展 bounded text 保存 role，验证 measurement 与 draw 一致。
4. 按 system surface、Launcher、各 App 迁移；每一批运行相关测试并检查 PNG。
5. 更新完整 snapshot hashes，运行 host/desktop/firmware gate，导出最终 contact sheet 并完成人工审阅。
6. 若需回退，可将角色映射临时全部指向保留的 legacy 6×10 descriptor；不回退 API 与资源审计契约。

## Open Questions

- 真机上 320×170 profile 的物理像素密度与观看距离仍需 P8 hardware review；在此之前 host 结论只能证明像素几何与相对层级，不能替代实体屏幕舒适度声明。
