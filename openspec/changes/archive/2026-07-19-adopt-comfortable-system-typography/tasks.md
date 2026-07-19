## 1. 资源、授权与可复现管线

- [x] 1.1 写入 Playdate/Roobert、Inter 与 Atkinson 的锁定版本、许可证、源码阅读、样张结论和采用/不采用决策研究记录
- [x] 1.2 纳入 Roobert 24 Medium、20 Medium、11 Bold、11 Medium、10 Bold、9 Mono Condensed 的最小源 `.fnt`/image table、CC BY 4.0 文本、SHA-256 与署名 manifest
- [x] 1.3 扩展 converter golden/stale/非法源输入/确定性/flash 报告用例，再生成包含六套字体的紧密 MSB-first C++ descriptor
- [x] 1.4 将六套生成资源接入 CMake 与 firmware shared source 审计，并让 `--check` 和 typography flash budget 成为构建门禁

## 2. Core typography 契约

- [x] 2.1 先增加 setup-time viewport role 解析、Hero/Footer metrics、动态 badge/button bounds、圆角 focus bounds、相同 viewport 确定性、kerning、白字反绘、clip/guard byte 与 native-scale 的失败测试
- [x] 2.2 实现平台无关 bitmap font descriptor、`TextRole` 与由 viewport 一次解析并缓存、但不读取 profile/设备的唯一角色映射
- [x] 2.3 让 `MonoCanvas::measureText`、alignment 和 text raster 使用显式 role 与同一 descriptor，增加共享 `fillRoundedRect` focus primitive，并保留可诊断边界行为
- [x] 2.4 先增加 bounded text role 保真、fit/wrap/ellipsis/marquee 与无越界失败用例，再让 request/result/layout/draw 全程保存同一 role
- [x] 2.5 更新产品静态审计，禁止 typography resolver 读取 display profile/设备、禁止 App 自行按尺寸分支、禁止系统 role 由 legacy 6×10 整数放大模拟

## 3. 系统与 App 迁移

- [x] 3.1 更新全部静态和 bounded text 调用点的语义迁移表，记录 viewport 解析值并明确 Timer 数字等业务 Display typography 不迁移
- [x] 3.2 迁移 system surface、system menu、overlay、fallback/error surface，并修正 320×170 与 400×240 的 bounds、滚动或可见内容
- [x] 3.3 迁移 Launcher 与 App runtime handoff 文本，验证长 App 名、footer 邻接标签和 transition keyframe 无意外裁剪
- [x] 3.4 迁移 Clock、Motion、Settings、Gallery 及其他内置 App 文本，统一底部操作栏为 `Footer`、统一主要 menu focus 为圆角背景、让 Settings 复用 System Menu 状态图标，并删除产品代码对隐式默认字体和字号模拟的依赖

## 4. 回归、视觉审阅与完成门禁

- [x] 4.1 更新所有受影响的 metrics/glyph/framebuffer golden 与双 profile App/system surface snapshot hash，保留失败 PNG 输出
- [x] 4.2 增加确定性 typography visual matrix 导出工具，生成双 profile 全内置 App、system menu/overlay、长文本与反色状态 contact sheet
- [x] 4.3 人工逐帧审阅 contact sheet，记录并修正所有严重裁剪、重叠、细碎笔画、层级不一致、密度过高或反色错误
- [x] 4.4 运行 OpenSpec validate、font stale/size gate、完整 host test、SDL desktop build/smoke、PlatformIO T-Embed compile 和 `git diff --check`
- [x] 4.5 执行需求逐项完成审计，记录 P8 真机舒适度仍待实体设备验证的边界，并确保除此以外无未完成项
