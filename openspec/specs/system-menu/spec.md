# system-menu Specification

## Purpose
定义系统拥有的 System Menu：长按开闭、最小系统动作、有界 UI primitives、语义反馈，以及普通 App 不得改写菜单结构的边界。

## Requirements
### Requirement: 长按从任意 App 打开系统菜单
系统 SHALL 将共享 long-press gesture 解释为打开或关闭 System Menu，不要求 App 实现硬件处理；System Menu SHALL 由系统拥有且不进入 App catalog。

#### Scenario: 非 Home App 打开菜单
- **WHEN** active App 稳定且用户触发 long-press
- **THEN** System Menu 覆盖该 App，catalog/current AppId 不变且不开始 Home transition

#### Scenario: Home 打开菜单
- **WHEN** Home App active 且用户触发 long-press
- **THEN** System Menu 打开但省略冗余的 Home action，导航顺序只包含当前可用动作

### Requirement: System Menu 提供最小系统动作
首版 System Menu SHALL 只提供 Resume、Sound、Motion 以及仅在非 Home App 中出现的 Home action；动作和状态 SHALL 来自 stable enum/typed snapshot/command，不得持有平台 driver 或任意 callback。Connectivity、Device 等只读信息 SHALL 不为了填充空间而进入此高频菜单。

#### Scenario: 返回 Launcher
- **WHEN** 用户在非 Home App 的 Menu 中确认 Home
- **THEN** Menu 关闭并以 Back 语义启动到配置 Home AppId 的正常 transition

#### Scenario: 修改声音设置
- **WHEN** 用户确认 Sound 行
- **THEN** 系统提交现有 typed volume command并在同帧 render显示 commit 后值，Muted 时视觉反馈仍完整

#### Scenario: 不展示无动作的状态行
- **WHEN** System Menu 渲染
- **THEN** Connectivity、Device 等无直接动作的状态不占用菜单行或页脚，复杂状态留给独立系统页面

### Requirement: 系统菜单使用有限且一致的 UI primitives
System Menu SHALL 使用 stateless Panel、Header、MenuRow、Selection、Value 与 StatusIndicator primitives；primitives SHALL 只依赖 canvas、bounds 和显式 visual state，不得拥有 input、service、App、callback 或动态树。

#### Scenario: 双 profile 渲染
- **WHEN** 同一个 Menu model 分别在 320×170 和 400×240 profile 渲染
- **THEN** 两者以有界时间从右侧滑入不透明 Menu panel，mask 同步渐强，冻结 App 的左侧上下文经 mask 后仍可见，并保持相同项目顺序、selection 和动作语义，所有文字与边框在 framebuffer bounds 内且可读
- **AND** Header 与可容纳的短 action label 优先使用接近行高的大字号，长 label 由 bounded-text 确定性降级，value 保持次级字号

#### Scenario: 选择与控件状态
- **WHEN** 行被选择或未选择，Sound 为任意音量，或 Motion 开关变化
- **THEN** 每种状态都有不依赖声音的稳定视觉表达并通过 snapshot/golden 锁定

### Requirement: 菜单导航与提示音使用语义反馈
Menu selection 变化 SHALL 产生至多一次 Navigate；打开、关闭、返回 Home、设置修改和拒绝 SHALL 使用既有 Confirm、Back、Toggle/Boundary/Reject 语义，不得请求资源路径或自定义 waveform。

#### Scenario: 快速旋转菜单
- **WHEN** 单个 frame 的任意 turn delta 改变 Menu selection
- **THEN** selection 按有界规则移动且至多提交一次 Navigate，不补播被抑制的历史 tick

#### Scenario: Home 不出现冗余动作
- **WHEN** current App 已是 Home
- **THEN** Home action 不渲染且旋钮导航跳过该 action，不产生隐藏 selection 或 transition

### Requirement: 普通 App 不控制系统菜单结构
第一阶段普通 App SHALL 不能枚举、删除、重排系统条目或注入任意 View/同步 callback；任何未来 App contribution SHALL 通过独立规格定义有界 descriptor 与异步 result。

#### Scenario: App context 被审计
- **WHEN** core/App public headers 与 bundled App source 被检查
- **THEN** 不存在可修改 System Menu system items 或传入 renderer/callback 指针的 App API
