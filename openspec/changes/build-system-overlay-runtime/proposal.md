## Why

Cadenza 已有可移植 App 生命周期、固定容量 catalog、系统服务事务和统一提示音，但运行时只能渲染 active App 或 App transition，长按输入也直接跳回 Home，无法承载由系统拥有、覆盖任意 App、隔离输入且行为一致的菜单、对话框和短暂反馈。现在需要先建立受约束的 System Overlay 基础和首个 System Menu 垂直切片，使设备形成稳定的系统级交互入口，再在其上演进持久化、通知和可安装 App。

## What Changes

- 新增固定容量的 System Overlay Runtime，明确 base App、transient feedback、单个 interactive surface、persistent indicator 与 critical surface 的合成及优先级。
- 新增全局输入路由和完整按键序列所有权；Overlay 打开或关闭时，触发手势及其 release 不得泄漏到 App 或新 surface。
- 新增可移植的 System Menu 和最小系统 UI primitives，包括 panel、header、menu row、selection、value 与 status indicator；同一源码覆盖 320×170 与 400×240 profile。
- **BREAKING**：非 Home App 的长按不再直接触发 Home transition，而是打开 System Menu；返回 Launcher 成为 Menu 中的显式系统动作。
- System Menu 打开时冻结当前 App 的 update/input 与背景画面，但系统服务继续推进；关闭后恢复原 App，不产生 App exit/enter。
- 新增异步、类型化且有界的 surface 请求/结果与拒绝诊断；第一阶段只允许系统拥有的 Menu/Dialog，普通 App 不注入任意 View 或同步 callback。
- 新增调研记录、采用/不采用决策、输入时序回归、双 profile 视觉 golden、headless/desktop E2E 与 firmware 编译门禁。
- 本变更不实现持久化、完整通知中心、动态 App 安装、任意深度 Overlay stack 或通用 retained component tree。

## Capabilities

### New Capabilities

- `system-overlay-presentation`: 系统 surface 的固定层级、输入所有权、暂停/恢复、合成、容量、优先级与诊断契约。
- `system-menu`: 全局 System Menu、系统动作、最小 UI primitives、双 profile 布局和可访问性行为。

### Modified Capabilities

- `portable-runtime`: 将长按直接返回 Home 改为系统菜单入口，并定义 Overlay 期间 active App 的冻结与恢复生命周期。
- `desktop-simulator`: 将桌面长按 smoke 路径改为打开 System Menu，并验证菜单内返回 Launcher 与双 profile 组合。
- `interaction-sound-language`: 将原有长按 Back 语义改为 Menu 打开/关闭与菜单内显式返回 Launcher 的提示语义。
- `runtime-verification`: 将原长按直接返回覆盖迁移为 Overlay 输入所有权、暂停生命周期、转场 defer 与 Menu Home 回归。

## Impact

- 影响 `cadenza_core` 的输入语义、`AppRuntime` update/render 编排、frame coordinator、App context 和诊断词汇。
- 新增独立的 portable system presentation 状态与 renderer；`cadenza_apps` 不持有系统菜单实现。
- headless、SDL desktop、T-Embed 与 ESP-IDF candidate 组合根必须装配相同的 System Overlay 行为。
- 现有 long-press Home、runtime、App snapshot、desktop smoke 和输入测试需要迁移；现有 App 转场、系统服务事务、声音语义与 framebuffer 契约保持兼容。
- 调研参考锁定 Playdate SDK 3.1.0、LVGL v9.4.0 `c016f72d4c125098287be5e83c0f1abed4706ee5`（MIT）和 Flipper Zero 1.3.4 `ad2a80042349a0cc6e0a14541e985d798a89f389`（GPL-3.0）；Flipper 源码仅作思想参考，不复制实现。
