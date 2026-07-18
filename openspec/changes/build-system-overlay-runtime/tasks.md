## 1. 调研、基线与门禁

- [x] 1.1 锁定 Playdate SDK 3.1.0、LVGL 9.4.0、Flipper Zero 1.3.4、Qt Declarative 6.11.1、microui 与 NobleEngine 版本/commit、许可证和关键源码
- [x] 1.2 建立 System Overlay reference research 与 adoption decision，记录采用/不采用、Playdate failure cases 和待验证假设
- [x] 1.3 将可 clone 参考放入 gitignored `.research/references` 并新增 clone commit、文档证据和 build-dependency 审计
- [x] 1.4 保存并运行现有 input 与 AppRuntime long-press Home 基线测试，确认改动前公共行为
- [x] 1.5 记录两种 framebuffer、Runtime 对象和 firmware RAM/flash 基线，证明后续不新增第三个最大 framebuffer

## 2. 失败用例与核心状态模型

- [x] 2.1 先写 failing tests 覆盖 long-press opening release、closing held release、Menu turn 隔离和下一新序列恢复 App
- [x] 2.2 写 failing lifecycle/frame tests 覆盖 App update/render 冻结、无 exit/enter、SystemService 继续推进与同帧 commit 后 snapshot
- [x] 2.3 写 failing transition composition tests 覆盖 transition 中 Menu capture/defer、目标 stable frame 打开与 current AppId 确定
- [x] 2.4 写 failing capacity/rejection tests 覆盖 single interactive slot、invalid action、busy、transient queue 满与诊断计数
- [x] 2.5 定义封闭 SurfaceKind、SystemMenuItem、SurfaceRejection、diagnostics 和 fixed-capacity transient model

## 3. System Surface Coordinator

- [x] 3.1 实现 button sequence owner、awaiting-release arming、long-press open/close 和 input consumption
- [x] 3.2 实现 single interactive slot、deferred Menu request、open/action/close intents 和稳定拒绝策略
- [x] 3.3 实现 App suspend-with-snapshot 状态、frozen framebuffer 复用和无第三 framebuffer 的编译/size 断言
- [x] 3.4 实现 transient feedback 有界队列、注入时间 expiry 与 passive input behavior
- [x] 3.5 为 coordinator 增加 optional diagnostic sink 和 deterministic counters/high-water

## 4. 系统 UI primitives 与 Menu renderer

- [x] 4.1 实现 stateless Panel、MenuRow、Selection、VolumeIndicator、Switch 与 StatusIndicator primitives
- [x] 4.2 实现 320×170 与 400×240 的 SystemMenuLayout、visible row、bounded text 和 clip 规则
- [x] 4.3 实现 Resume、Sound、Motion 与非 Home 场景 Home action，并从高频菜单移除无动作的 Connectivity/Device 状态
- [x] 4.4 实现 frozen base → transient → interactive → indicator 的固定合成顺序
- [x] 4.5 生成并审核双 profile Menu、selection、Sound/Motion 控件与 toast/indicator snapshots/goldens

## 5. Runtime、服务与声音接入

- [x] 5.1 将 AppRuntime update/render 与 FrameCoordinator 接入 coordinator，移除 long-press 直接 Home shortcut
- [x] 5.2 实现 Menu Resume/Home，保证 Home 走配置 Home AppId 的正常 Back transition
- [x] 5.3 将 Sound/Motion action 通过现有 typed SystemCommand 提交并让同帧 Menu render读取 commit 后 snapshot
- [x] 5.4 确认 Connectivity/Device 不进入高频菜单，也不向 presentation 暴露 credential/platform handle
- [x] 5.5 接入 Navigate、Confirm、Back、Toggle、Boundary/Reject 语义并验证 Muted 视觉等价反馈
- [x] 5.6 更新 headless、desktop、T-Embed 与 ESP-IDF candidate composition roots，保证同一 portable surface path

## 6. 自动化、E2E 与质量门禁

- [x] 6.1 通过 coordinator、input、runtime、frame trace、capacity 和 diagnostics 全部单元/集成测试
- [x] 6.2 更新 App/runtime/desktop smoke 路径，脚本访问每个 App 并通过 System Menu Home 返回 Launcher
- [x] 6.3 通过双 profile visual golden、headless deterministic replay 与 SDL dummy real executable E2E
- [x] 6.4 通过 strict warnings、sanitizer、core-alone/shared-source/research audit 与 `git diff --check`
- [x] 6.5 通过当前 PlatformIO T-Embed firmware 和锁定 ESP-IDF candidate 编译，记录 RAM/flash/task/stack 差异
- [x] 6.6 严格校验 OpenSpec 并更新 platform architecture、verification、development 与用户交互文档

## 7. 实体交互验收

- [ ] 7.1 在实体 T-Embed 验证 650 ms 长按、触发 release 不误确认、关闭 held release 不泄漏和快速连续操作
- [ ] 7.2 在实体 320×170 屏验证 Menu 可读性、旋钮选择、声音/静音反馈和至少 100 次 open/close 稳定性
- [ ] 7.3 根据真机结果决定是否只调 InputConfig 阈值/视觉常量；任何 contract 变化回写 design/spec 并重跑完整门禁
