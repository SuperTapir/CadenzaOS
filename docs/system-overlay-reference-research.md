# System Overlay 与系统菜单参考调研

> 状态：源码级参考、版本/许可证锁定、失败案例、采用边界和 Cadenza 软件实现验证已完成；真机手感仍由 `build-system-overlay-runtime` 的实体硬件任务证明。

本文记录 System Overlay、系统菜单、modal 输入隔离和轻量 UI primitives 的上游证据。可 clone 的参考仓库均浅克隆到被 gitignore 的 `.research/references/`，只用于复查，不是产品构建依赖。Playdate OS 不开源，因此锁定公开 SDK 3.1.0 文档和官方 changelog，不能把其内部实现当作已审计源码。

## 1. 研究问题

本轮不只比较“如何画一个弹窗”，而是验证：

1. 系统 surface 与普通 App/scene 如何分层；
2. modal 打开和关闭时 Press、LongPress、Release 的 owner 如何保持一致；
3. App 是继续 update、暂停、退出还是只冻结画面；
4. App 能向系统菜单贡献什么，系统如何保持所有权；
5. 组件采用 retained tree、immediate mode 还是 stateless drawing primitive；
6. 固定容量嵌入式设备如何处理 stack、queue、focus 和拒绝；
7. 哪些历史故障证明边界不完整会真实出错。

## 2. 固定参考矩阵

| 项目 | 固定版本 / commit | 许可证 | 本地 clone / 证据 | 直接阅读的关键源码 |
|---|---|---|---|---|
| Playdate SDK | 3.1.0 | SDK/OS 为 Panic 产品许可；这里只引用公开 API/设计文档 | [Inside Playdate 3.1.0](https://sdk.play.date/3.1.0/Inside%20Playdate.html)、[Designing for Playdate](https://sdk.play.date/1.9.3/Designing%20for%20Playdate.html)、[SDK Changelog](https://sdk.play.date/changelog/) | `gameWillPause/gameWillResume`、`getSystemMenu`、三类 menu item、`setMenuImage`；OS 源码不可得 |
| LVGL | v9.4.0 / `c016f72d4c125098287be5e83c0f1abed4706ee5` | MIT | `.research/references/overlay-lvgl` | `src/display/lv_display.c`、`src/indev/lv_indev.c`、`src/widgets/msgbox/lv_msgbox.c` |
| Flipper Zero firmware | 1.3.4 / `ad2a80042349a0cc6e0a14541e985d798a89f389` | GPL-3.0 | `.research/references/overlay-flipper` | `applications/services/gui/gui.c`、`view_stack.c`、`view_holder.c`、`applications/services/dialogs/dialogs_module_message.c` |
| Qt Declarative | v6.11.1 / `a02bed441965ee1f18f856352c7d5ee5ba35d795` | Commercial OR LGPL-3.0-only OR GPL-2.0-only OR GPL-3.0-only（文件 SPDX） | `.research/references/overlay-qtdeclarative`（partial/sparse clone） | `src/quicktemplates/qquickpopup.cpp`、`qquickoverlay.cpp`、`qquickpopupwindow.cpp`、`qquickshortcutcontext.cpp` |
| microui | `0850aba860959c3e75fb3e97120ca92957f9d057` | MIT | `.research/references/microui` | `src/microui.h`、`src/microui.c` 的固定 pool、root z-order、focus、popup close |
| NobleEngine | `93ffd6eb29d61c1af1cb8d40b78daf75c4e85946` | 代码/大部分资产 MIT；logo 与 bundled libraries 另有边界 | `.research/references/NobleEngine` | `Noble.lua`、`modules/Noble.Input.lua`、`modules/Noble.Menu.lua` |

## 3. Playdate：系统拥有 Menu，游戏只贡献受限描述

Playdate 有独立 Menu button。官方 SDK 3.1.0 规定系统在 Menu button 被按下前调用 `playdate.gameWillPause()`，关闭前调用 `playdate.gameWillResume()`。游戏最多向 System Menu 加三个条目，类型封闭为 action、checkmark、options。系统条目不会被 `getMenuItems()` 返回，也不能被游戏删除。Action 选择后顺序为：OS 隐藏 Menu、调用游戏 callback、恢复游戏并调用 `gameWillResume()`；check/options callback 也延迟到 Menu 关闭后、resume 前执行。

`playdate.setMenuImage()` 允许游戏提供 400×240 图像，但重要内容只能放左侧 200 px，右侧由系统菜单遮挡；这说明 App 可以贡献 context image，却不拥有 Menu 的 layout、input 和 animation。

用户提供的本地只读参考 `~/Development/playdate-design-reference` 保存了 2026-07-18 收集的官方 SDK 3.1.0《Inside Playdate》离线快照、图片清单和设计笔记。该目录不是 Git 仓库，因此用来源版本与 SHA-256 锁定关键证据：`source/Inside-Playdate.md` 为 `2f427d968828cb6edca6de5f63de3c61cf7e6d4c243a093ff9eda07931710e05`，离线 HTML 为 `6ec0f6ba80e86fb1aae442e5f7598f186e510e3dab7a53e2d790cde18e6b0e62`。其中截图、文档与品牌资产均归 Panic Inc.，只作内部研究，不复制为产品资产。

这份快照进一步确认了视觉/交互约束：游戏最多贡献三个自定义项；条目只有 action、checkmark、options 三类；options 的标题和值必须保持短；高频 System Menu 必须短、浅、可扫读。结合用户提供的真机菜单截图，本轮采用的是信息层级而非像素临摹：右侧半屏抽屉、冻结 App 留在左侧、可操作项进入主列表、Sound 用始终可见的实心/空心音量格、Motion 用横向开关、无动作状态不占菜单空间、Home 页面省略冗余 Home action。

官方设计文档同时明确：Playdate 的系统 UI 刻意很少，Panic 不向游戏提供大量统一 widget；游戏保持自己的视觉，系统只提供 Setup、Home、Menu、Settings、keyboard、crank indicator 等少量一致入口。这是 Cadenza 区分“系统 surface policy”和“普通 App 组件风格”的直接依据。

### 3.1 官方 changelog 中与 Overlay 直接相关的失败案例

官方历史修复至少包括：

- System Menu 打开时按钮仍 held，游戏留下 stuck-down 状态；
- Menu button 音量快捷键 press 泄漏到游戏；
- Menu 关闭期间快速按键使设备 stuck；
- Menu callback 在 `playdate.wait()` 中崩溃；
- 动态 add/remove menu item 导致 crash；
- 锁屏时 Menu 仍打开导致 weird state 或 display garbage；
- 游戏 display scale/invert/offset 污染系统 Menu 或 network permission dialog；
- `gameWillPause/gameWillResume` 中暂停/恢复 fileplayer 导致返回 Menu 挂起；
- System Menu、Launcher animation、notification 与 Catalog/Unwrap 等系统 surface 组合触发 crash 或 visual glitch。

这些失败不是视觉问题，而是 input sequence、drawing state、callback execution phase、surface composition 和 lifecycle 边界问题。Cadenza 的测试必须把它们转成输入/帧 trace，而不是只做 screenshot。

### 3.2 不能直接照搬的部分

Playdate 有 Menu、A、B、d-pad 和 crank；Cadenza 原型只有 encoder turn + 单按钮。Cadenza 不能假设独立 Menu key，也不能让 opening long-press 的 release 自动成为 Menu confirm。Playdate OS 源码不可审计，因此只能采用可观测契约，不推断其内部数据结构或许可证可复用性。

## 4. LVGL：永久 top/system layer 与自顶向下命中

LVGL v9.4.0 在 `lv_display_create()` 中为每个 display 创建 bottom layer、active screen、top layer、system layer。Top/system 默认透明且不可点击；应用可以使 top layer clickable，从而吸收下层点击。

`pointer_search_obj()` 的命中顺序固定为：

```text
system layer → top layer → active screen → bottom layer
```

`lv_msgbox_create(NULL)` 会在 top layer 自动创建全屏 backdrop，再把 message box 放在 backdrop 中。这证明 modal 的输入遮罩是 surface tree 的一部分，而不是只在业务 callback 里“尽量不处理下层输入”。

采用：固定层级、显式 top-down hit order、modal backdrop、per-display ownership。

不采用：LVGL object tree、style/theme system、dynamic widget allocation 和完整 input device framework。Cadenza 已有 `MonoCanvas`、动画、bounded text 与单输入模型，引入 LVGL 会形成第二套 renderer/lifecycle 并扩大 RAM/flash。

## 5. Flipper Zero：层级优先级与完整按键 owner

Flipper GUI service 定义 Desktop、Window、StatusBarLeft/Right、Fullscreen。Draw 和 input 都选择最高优先级 enabled ViewPort；Fullscreen 存在时 Window/Desktop 不接收输入。

更关键的是 `gui_input()`：第一次 `InputTypePress` 时锁存 `ongoing_input_view_port`。即使按住期间 top ViewPort 改变，Release 仍交给旧 ViewPort；其他中间事件不会错误交给新 ViewPort。`ViewStack` 固定 `MAX_VIEWS = 3`，draw 从底到顶，input 从顶到底直到 consumed。

Dialogs service 通过消息 queue 和 `ViewHolder` 在 Fullscreen layer 展示 `DialogEx`。调用者可以同步等待结果，是因为 Flipper 有独立线程和 service record；这与 Cadenza 单 frame coordinator 不同。

采用：完整 Press→Release owner、最高 enabled layer 优先、有限 stack、system-owned dialog service。

不采用：同步阻塞 dialog API、动态 `malloc` object graph、多线程 GUI service、GPL-3.0 代码和命名。Flipper 只作思想参考，Cadenza 必须独立实现。

## 6. Qt Quick：modal、close policy 和 input grab 是不同维度

Qt `QQuickPopup` 把 `modal`、`dim`、`focus`、`closePolicy`、open/close transition 分开建模。`QQuickOverlay` 按 stacking order 分发 overlay event，并显式吞掉被 modal 遮挡的 wheel/tablet/pointer event；`QQuickPopupWindow` 管理 mouse/keyboard grab，并分别处理 outside press 与 outside release 的 close policy。

Qt 源码里围绕 grabber、release-outside、modal shadow、多个 popup stacking、shortcut context 有大量专门逻辑，证明“一个 bool visible + z-order”不足以成为成熟 Overlay contract。

采用：把 modality、input ownership、close policy、focus/selection 和 visual dimming 分成显式语义。

不采用：多 window/native popup、任意 popup stack、QML object model、pointer/touch/hover 全套 delivery。Cadenza 当前只有单 display、encoder 与按钮，应选择更窄的封闭状态机。

## 7. microui：Immediate mode 并不等于低内存或系统 policy

microui 2.02 使用 immediate API，但 `mu_Context` 默认内含 256 KiB command list、32 个 root/container/clip/id stack、48 个 container pool 和 retained focus/hover/z-index state。Popup 只是 root container 的一个 option：打开时 bring-to-front，点击其他 root 时关闭。

采用：stateless call-site、fixed pool、显式 clip、selected/focused state由 caller/model 驱动。

不采用：256 KiB command list、mouse/desktop window assumptions、通用 root sorting、用 GUI focus 代替 system input owner。这个参考反证“immediate mode”标签本身不能保证适合 ESP32 或保证系统级输入隔离。

## 8. NobleEngine：社区也主动限制 input stack

NobleEngine 建立在 Playdate SDK 上。其 `Noble.Input` 明确承认 Playdate 能 push 多个 input handler，但选择只维护一个 active handler，并警告手工 push/pop 会产生 unexpected behavior。scene transition start 时会清空 input handler，transition 完成后恢复新 scene handler。`Noble.Menu` 是 App 内 `gridview` 封装，拥有 callback 和动态 item，属于游戏 UI，不是 System Menu。

采用：一个 active input owner、transition 期间 suspend input、菜单视觉 primitive 与系统 Menu policy 分离。

不采用：Lua callback menu 直接作为系统 API、按 scene push/pop 任意 handler、运行时动态表作为嵌入式系统 contract。

## 9. 跨项目共同结论

```text
系统入口/策略        系统拥有
App 内容/状态贡献    受限 descriptor 或 frozen image
视觉 layer           固定顺序
modal input           top owner，禁止穿透
Press→Release         同一个 owner
close/action          明确 phase，不在 draw 重入
stack/queue           有界
App navigation        不等于 System Overlay
```

不存在一个适合直接引入的完整库：Playdate 最符合产品语义但 OS 闭源；LVGL/Qt 太宽；Flipper 的线程、分配和 GPL 边界不适合；microui 的默认内存和 desktop focus 模型不适合；NobleEngine 只解决 Playdate App scene/UI。Cadenza 应独立实现一个更窄的 system presentation state machine，并复用现有 `MonoCanvas`、`InputReducer`、App lifecycle、SystemService transaction 和声音语义。

## 10. 待实现验证的关键假设

1. 现有单按钮 `InputFrame` 的 `longPressed` 与后续 `released` 足够用 `awaitingRelease` 完成 sequence capture，无需先扩张 raw event ABI。
2. Runtime 可在 Menu 打开时复用 transition framebuffer 保存 frozen App，不增加第三个最大 12,000-byte framebuffer。
3. Menu active 时跳过 App update/render，但继续 `SystemServiceHost beginFrame/commitCommands`，可以保持同帧 settings snapshot 与后台 service 进度。
4. Transition 期间的 Menu long-press 可以捕获并 defer 到 stable target App，而不改变 transition lifecycle。
5. stateless primitives 和固定 menu model 能在 320×170 与 400×240 保持相同语义与可读性。

这些假设只有失败测试、frame trace、双 profile golden、size/build 和真机输入手感通过后才算成立。
