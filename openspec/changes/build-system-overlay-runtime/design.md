## Context

Cadenza 的 portable runtime 当前只有 active App 与 App transition 两种 presentation state。`InputReducer` 将旋钮和单按钮规约为 `InputFrame`，`AppRuntime::updateWithSystem()` 在非转场时优先检查 `longPressed` 并直接启动 Home transition，随后才把输入交给 App。`renderWithSystem()` 也只渲染 App 或 transition，没有独立 system surface 合成点。

现有 system service 已采用冻结 snapshot、typed command、固定容量 FIFO 与确定 frame transaction；Overlay 必须保持同样的无逐帧分配、可移植、可诊断和 headless 可重放边界。目标硬件只有旋钮和一个按钮，没有 Playdate 的独立 Menu 键；触发 Overlay 的长按序列必须与 Menu 后续点击严格隔离。

调研锁定 Playdate SDK 3.1.0、LVGL v9.4.0 commit `c016f72d4c125098287be5e83c0f1abed4706ee5`（MIT）、Flipper Zero firmware 1.3.4 commit `ad2a80042349a0cc6e0a14541e985d798a89f389`（GPL-3.0）、Qt Quick 6.11.1 与 Pebble/Rebble SDK 文档。Playdate 证明系统拥有 Menu、暂停 App 并只接受受限声明式扩展；LVGL 证明永久 top/system layer 与自顶向下命中；Flipper 证明 Press→Release 绑定同一 ViewPort；Qt 证明 modal、focus、dim 与 close policy 必须是一等语义。Cadenza 采用这些契约思想，但不复制 GPL 源码，也不引入 retained widget dependency。

## Goals / Non-Goals

**Goals:**

- 建立固定容量、确定性的 system surface coordinator 和渲染合成点。
- 将完整输入序列交给唯一 owner，防止 Overlay 边界输入泄漏。
- 交付一个由系统拥有的 System Menu，替代长按直接返回 Home。
- System Menu 期间冻结 App update/input/背景，但保持 system services 正常推进。
- 提供足以构建 System Menu、后续 Dialog 与 Toast 的最小 stateless primitives。
- 保持 320×170、400×240、headless、SDL、T-Embed 与 ESP-IDF candidate 行为一致。

**Non-Goals:**

- 不建立任意 retained component tree、DOM/reconciliation、动态布局树或通用 focus engine。
- 不允许 App 注入任意 View、同步 callback 或覆盖系统条目。
- 不交付持久化、完整通知中心、动态 App、任意深度 modal stack 或平台原生窗口。
- 不改变系统服务事务、App transition 算法、声音 palette 或 framebuffer 格式。

## Decisions

### 1. Overlay 是 Runtime presentation state，不是 bundled App

新增 `SystemSurfaceCoordinator` 作为 portable runtime 的组合成员或协作者。它拥有 surface state、输入路由和系统菜单模型；`cadenza_apps` 只保留普通 App。这样 Menu 不进入 catalog，不参与 AppId、launcher visibility、capability 或 App enter/exit。

备选是注册隐藏的 SystemMenu App。该方案会把暂停当前 App伪装成 App transition，产生错误的 exit/enter、Home/catalog 语义，也无法自然覆盖冻结背景，因此不采用。

### 2. 固定四层语义，但第一版只激活一个 interactive slot

合成顺序固定为：

```text
base App / transition
  → transient feedback
  → interactive surface (System Menu 或未来 Dialog，capacity = 1)
  → persistent/critical system indicator
```

第一版实现 `None` 与 `SystemMenu` interactive kind，并保留封闭枚举和拒绝诊断供未来 Dialog 扩展。Transient 使用小型固定队列；无内容时不增加 framebuffer。不得使用任意深度 stack。

单 slot 能消除 modal 间焦点、z-order 与回退歧义，代价是并发请求必须 reject/defer。首版对普通请求使用 deterministic reject；critical surface 后续可定义 replace policy，不在本 change 虚构优先级。

### 3. 输入所有权按完整按钮 sequence 锁存

现有 `InputFrame` 是帧级语义而非原始 event queue。Coordinator 维护 `ButtonSequenceOwner {None, App, System}` 与 `awaitingRelease`：

- 按钮空闲时，active interactive surface 存在则 owner 为 System，否则为 App。
- `longPressed` 无论当前 App 是否 Home 都转移为 System 请求并打开/关闭 Menu；触发 frame 的全部 input 被消费。
- Menu 因 long press 打开后保持 unarmed，直到看到该 sequence 的 `released`；该 release 只用于完成 capture，不执行菜单动作。
- Menu 内短点击完整归 System；Menu 关闭所用 sequence 的 release 也不得交给 App。
- turn 在 Menu active 时只交给 Menu；否则交给 App。

备选是只按每帧“当前谁在最上层”路由。这会复现 Playdate/Flipper 历史中的 stuck button 与 release leakage，因此不采用。

### 4. System Menu 使用 suspend-with-snapshot，不改变 App 生命周期

打开 Menu 时 Runtime 保存 active App 已提交的 framebuffer 作为 frozen background。Menu active 期间：

- active App 不收到 update 或 input；
- App 不收到 `onExit`，关闭后也不收到 `onEnter`；
- system `beginFrame`、platform event ingestion、command commit、audio/connectivity/voice service 继续运行；
- render 从 frozen framebuffer 开始，再绘制 Menu 和 indicators；不调用 App render。

首版不新增 `onSystemPause/onSystemResume`，避免没有真实 consumer 时扩大长期 App ABI。未来若 App 必须暂停自己拥有的非系统资源，再以独立 requirement 增加 typed suspend notification。

Menu 在 App transition 期间的长按请求采用 defer-to-stable-frame：立即捕获该输入序列，待 transition 完成后的首帧打开 Menu并冻结最终目标 App。这样既不让输入泄漏，也不需要冻结半完成的 transition composition。critical fault UI 不受此普通策略约束，后续另定义。

### 5. System Menu 是声明式固定模型，不执行 callback

第一版固定可交互项为 Resume、Home、Sound、Motion，其中 Home 只在当前 App 不是 Home 时出现。实际行为为：

- Resume：关闭 Menu；
- Home：仅在当前不是 Home 时出现，关闭 Menu 并启动 Home transition；
- Sound：提交现有 `SetSoundVolume` typed command；
- Motion：提交现有 `SetMotionProfile` typed command；
- Connectivity 与 Device：不进入高频菜单；未来需要详情时进入独立系统页面，不暴露平台 driver。

菜单项使用 enum id 和固定数组；renderer 读取 model 并调用 stateless primitives。选择动作写成 coordinator intent，在 frame coordinator 的明确阶段提交，不在 render 或同步 callback 中重入 service。

App 自定义 menu item 留到真实需求出现后再设计受限 descriptor、capacity 与异步结果。

### 6. System UI primitives 保持无状态、无所有权

新增 Panel、Header、MenuRow、Selection、Value、StatusIndicator 等绘制函数或轻量值对象，只接收 `MonoCanvas`、bounds 与显式 visual state。它们不持有 service、App、input、heap、global theme 或 callback。

布局由一个 `SystemMenuLayout` 根据 canvas profile 计算，使用半开 Rect、可见行数和 Playdate 式贴右边缘单分隔线；文本统一走 bounded text。Menu 是从右侧在 160 ms 有界时间内滑入并覆盖冻结 App 的不透明 drawer，mask 随 reveal progress 渐强且只叠加黑点，不得改写冻结帧的其余像素；drawer 外仍显示降权后的 App 上下文。首版不绘制标题栏、selection 箭头或无动作页脚；选中行只使用内缩黑底白字，Sound 始终显示完整柱状轮廓并以实心/空心表达音量，Motion 使用紧凑横向开关。动画第一帧起输入即归 System。400×240 保持相同的紧凑行高和操作语义，不为填满面板而拉伸行。

### 7. 声音由语义动作触发，视觉反馈始终存在

- 打开 Menu：Confirm；
- 关闭/Resume：Back；
- 选择变化：Navigate，沿用抑制策略；
- Home：Back 并启动现有 transition；
- Sound/Motion 修改：现有 Toggle/Confirm 语义；
- disabled/busy：Boundary 或 Reject。

声音命令继续通过 `SystemCommandSink`，Muted 时所有状态仍由 selection、value 与 disabled style 表达。

### 8. 调研与验证证据进入仓库

保存 `docs/system-overlay-reference-research.md` 与 `docs/system-overlay-adoption-decision.md`，记录版本/commit、许可证、关键源码、采用/不采用项与 Playdate failure cases。自动化至少包括 input sequence、runtime lifecycle、frame ordering、capacity/reject、双 profile snapshots、headless/desktop smoke、strict warnings、sanitizer、source audit 与 firmware/candidate compile。

## Risks / Trade-offs

- [Risk] 长按打开 Menu 与原直接 Home 形成行为破坏 → 更新 portable/desktop/sound specs、smoke 路径和用户可见 Menu Home 动作，不保留隐式双语义。
- [Risk] 帧级 `InputFrame` 缺少 raw sequence id，复杂多键环境可能不足 → 当前物理模型只有一个按钮，使用 owner + awaitingRelease 可完整覆盖；未来多按钮再把 sequence id 下沉到 reducer。
- [Risk] 冻结 App update 会让 App 自管计时暂停 → 这是 System Menu 的明确语义；system services 使用独立 monotonic/simulation transaction继续推进，并以测试锁定。
- [Risk] Menu 改设置需同帧显示 commit 后值 → action 只提交 typed command，render 读取 commit 后 snapshot，复用现有 frame transaction。
- [Risk] Overlay 与 transition 组合出现半帧或输入丢失 → 普通 Menu 请求在 transition 期间捕获并 defer，到 stable frame 才打开；写精确 trace tests。
- [Risk] primitives 过早泛化形成第二套 GUI framework → 只实现 System Menu 实际使用的 stateless primitives；未来组件以真实 surface 驱动扩展。
- [Risk] 双 framebuffer snapshot 增加 RAM → Runtime 已持有 transition outgoing/incoming framebuffer；优先复用稳定 frame storage，不新增第三个 12 KiB framebuffer，并用 size report证明。
- [Risk] Flipper GPL 实现污染 → 研究文档记录许可证，只复用公开思想和独立实现，不复制代码或结构命名。

## Migration Plan

1. 保存现有 long-press Home、input、runtime、snapshot、desktop smoke 与 firmware size 基线。
2. 添加 research/adoption 文档和 source/许可证审计。
3. 先用失败测试锁定 sequence capture、suspend-with-snapshot、transition defer 与单 slot rejection。
4. 引入 coordinator、surface types 和 primitives，不接入默认长按路径；通过 core/headless tests。
5. 将 AppRuntime/frame coordinator 接入 Overlay，并迁移长按语义、System Menu action 和声音。
6. 更新双 profile golden、desktop smoke、firmware/candidate composition 与全部文档。
7. 运行完整自动化/构建门禁；实体硬件仅需验证旋钮长按手感、屏幕可读性和输入无泄漏，不以模拟器代替交互手感结论。

回滚以恢复旧 composition path 和 long-press Home tests 为单位；新 coordinator 未成为唯一入口前不删除旧行为。不得以把 SystemMenu 注册成 App 作为回滚补丁。

## Open Questions

- 真机上长按阈值 650 ms 是否适合频繁打开 Menu，需在 T-Embed 上试用后决定是否调整；API 不硬编码具体毫秒值。
- Connectivity/Device 未来是否进入独立只读系统页，待有真实详情需求后另行定义；不得因此暴露平台 driver。
- Future Dialog 是否与 System Menu 共用完全相同的 suspend policy，待首个危险确认操作出现时用独立场景验证。
