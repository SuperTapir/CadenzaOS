# App Handoff 与 System Menu 动效参考调研

日期：2026-07-19
适用 change：`align-app-cover-handoff`

## 结论

- Launcher 只拥有从轨道/card chrome 到 App 身份的系统交接；App 入场后半段不是统一模板，而是每款 App 独立的 launch sequence。
- 静态 Cover 与 launch sequence 必须分开：Cover 继续是不可变的目录身份，launch renderer 是可 seek、无 lifecycle side effect 的全屏纯绘制能力。
- 返回 Launcher 不倒放各 App 的入场动画，而使用当前 App 的静态 Cover bridge，形成更快的“收回卡片”语义。
- System Menu 是独立 system surface：右侧面板快速滑入、完整滑出；关闭期间底层 App 继续冻结且输入仍属于 Menu。

## 四段视频逐帧证据

| 文件 | 尺寸 / 时长 | 关键时间点 | 结论 |
|---|---:|---|---|
| `PixPin_2026-07-19_00-34-52.mp4` | 460×452 / 5.75 s | 约 2.85 s 启动；3.00 s chrome 退场；3.15–3.20 s dither 到黑；3.65 s App 首屏 | 系统先完成 Card/Launcher 交接，再揭示 App |
| `PixPin_2026-07-19_00-46-18.mp4` | 660×654 / 5.625 s | 约 1.25 s pressed 反馈；1.75–1.95 s chrome 退场；2.05–3.65 s `MUSIC BOX` 专属标题/扫描线；3.75 s 曲目 UI | launch sequence 属于 Music Box 自身，不是统一系统缩放 |
| `PixPin_2026-07-19_00-47-57.mp4` | 622×376 / 5.792 s | 约 1.85 s Launcher chrome 消失；人物画面继续占满屏幕并持续变化 | 另一 App 使用完全不同的内容连续入场 |
| `PixPin_2026-07-19_00-50-54.mp4` | 946×506 / 4.5 s | 约 1.25–1.35 s 右侧菜单滑入；约 3.35–3.65 s 向右滑出 | Menu close 不是瞬切；底层 App 在全过程冻结可见 |

抽帧使用本机 FFmpeg 7.1，以 10 FPS contact sheet 和转场区间 20 FPS 复核；时间点用于确定阶段与数量级，不作为未经真机批准的永久毫秒 golden。

## 锁定上游契约

本机 Playdate SDK 固定为 1.12.3。`Inside Playdate.html` 明确规定：

- `card.png` 为 350×155；
- Launcher 在启动动画前把 card 放在 400×240 的 `(25, 43, 350, 155)`；
- `launchImages/1.png, 2.png, ...` 是每个 game 自己提供的 400×240 全屏 transition frames；
- `launchImage.png` 是 loading/末帧 fallback。

`Examples/Level 1-1/Source/SystemAssets` 的 `card.png`/`card-pressed.png` 均为 350×155，`launchImages/1..9.png` 均为 400×240；关键源码和资产尺寸已直接检查，没有只依赖 README。Cadenza 只采用“静态 Card -> 每 App 全屏序列 -> 首屏”的职责划分，不复制 SDK 示例像素、声音或品牌。

## 既有开源证据复用

本 change 复用 `docs/reference-research.md` 已锁定的源码级调研：

- NobleEngine `93ffd6eb29d61c1af1cb8d40b78daf75c4e85946`：采用 Cover transition 的中点 lifecycle 交接思想；不采用 Lua scene engine、logo 或 transition assets。
- Taxman Engine `51981885dcb9bb7b467cb578785ac68a084fd8ab`：采用 off-screen/dither 对场景合成有价值的结论；不采用 `calloc/realloc` render textures 与完整 ECS。
- LVGL `a73e59f6d144785012e44ab93f1f3d4dde48a180` / Tweeny `d50d96dfa61b115fb0d9fce59f7bbe4488f9b9b0`：采用可 seek/确定性时间语义；不采用动态 descriptor/callback 容器。

## 采用 / 不采用

采用：App 独立 const launch renderer、归一化 progress、系统中点 lifecycle、两张固定 framebuffer、ordered dither、Menu 显式 closing 状态、双 profile 快照与真机 P8。

不采用：给静态 Cover 增加 launching 状态、运行时缩放已二值化 Cover、第三张全屏 transition buffer、提前 `onEnter`、外部 PNG/PDI 文件协议、倒放 App launch sequence 作为返回、Menu close 瞬切。

## 2026-07-19 实现与验证记录

实现采用两阶段 handoff：默认 Launcher → App 为 0.80 秒，App → Launcher 为 0.44 秒；Reduced Motion 分别为 0.56 秒和 0.28 秒。Clock、Motion、Settings、Animation Gallery 均提供独立的代码原生全屏 launch renderer；未实现该能力的 App 使用居中的静态 Cover/受约束名称卡。System Menu 在右边缘锚定，逐 scanline 形成斜向前缘，并把完整面板横向重采样进当前可见宽度；Resume 关闭期间仍冻结 App、消费输入，Home 则直接交给 return handoff，避免叠加两条系统动画。

自动化验证结果：

- `cmake --build build -j4`：通过；
- `ctest --test-dir build --output-on-failure`：70/70 通过；
- 双 profile golden：四个内置 App 入场、无专属 renderer fallback、Clock 返回、Menu opening/closing 均已生成 PNG 目检并锁定 hash；
- `sizeof(AppRuntime) < 25,000`、两 framebuffer 复用、lifecycle 中点、输入冻结、一次性 sound/diagnostic、共享源码审计和 `git diff --check` 均由测试或审计通过；
- PlatformIO compile 与独立 strict configure 在本轮被 macOS 以 `SIGKILL`/无法启动 Python 终止，系统同时存在多个 0.4–2.3 GB 常驻进程；没有编译诊断可归因于本 change，释放主机内存后仍须复跑，不能记为通过。

自动化只批准像素确定性和系统语义。T-Embed 真机仍需以 30 FPS 检查 Memory LCD 的 dither 闪烁、拖影、0.80 秒入场是否拖沓、0.44 秒返回是否干脆，以及菜单 0.16 秒形变是否与参考视频一致；这些是 P8 人工视觉批准边界，host PNG 不能代替。
