## 1. 参考证据与失败用例

- [x] 1.1 记录四段参考视频逐帧时间点、Playdate SDK 1.12.3 card/launchImages 契约、既有 Noble/Taxman 决策与采用/不采用边界
- [x] 1.2 先增加 App launch renderer 的 const/可 seek/双 profile/内置差异/fallback 失败测试
- [x] 1.3 先增加 staged handoff 的中点连续、两 buffer 复用、方向路由、lifecycle 与输入冻结失败测试
- [x] 1.4 先增加 System Menu closing 单调进度、输入所有权、冻结释放、一次性 sound/diagnostic 与 Home 互斥失败测试
- [x] 1.5 增加四 App 双 profile 的 launch p0=Cover、p1=首屏与 30 FPS 相邻像素变化上限失败测试

## 2. App launch sequence 契约

- [x] 2.1 在 App 与 AppCatalogView 实现可选纯绘制 launch-frame renderer，不改变静态 Cover 或 lifecycle API
- [x] 2.2 实现共享的双 profile Cover bridge/fallback 布局，保证与 Launcher 中央 Cover 首帧对齐且零几何诊断
- [x] 2.3 为 Clock、Motion、Settings、Gallery 实现四套不同的代码原生 1-bit launch sequence，并锁定代表关键帧
- [x] 2.4 让 launch renderer 接收只读 AppRenderContext，并为四 App 抽取与真实首屏共享的绘制 helper
- [x] 2.5 将四套独立海报式序列替换为 Cover 到真实首屏的连续 ordered-dither/motif 演变

## 3. Runtime staged handoff

- [x] 3.1 为 Transition 增加默认 direct、可选 staged 的 frame-model capability，并实现 Launch/Return handoff 策略
- [x] 3.2 在 AppRuntime 中实现 pre-midpoint launch/bridge 重绘、中点 buffer 搬移、post-midpoint 首屏合成且不增加第三 framebuffer
- [x] 3.3 实现默认 Home 方向路由、Normal/Reduced Motion 时序与显式自定义 transition 兼容路径
- [x] 3.4 更新默认时长相关 lifecycle、headless、desktop 与 system-surface 集成测试

## 4. System Menu 可逆开合

- [x] 4.1 在 SystemSurfaceCoordinator 中实现 opening/open/closing/released 状态和关闭期间输入/冻结所有权
- [x] 4.2 为面板滑入使用 ease-out、滑出使用 ease-in，并用 caller-owned scratch 实现确定性斜前缘 scanline 压缩形变，保证底层冻结 frame 与 dither mask 连续
- [x] 4.3 保持 Resume/close sound 与 diagnostic 一次性，Home 直接交给 App return handoff 而不叠加 Menu close 动画
- [x] 4.4 更新双 profile Menu opening/open/closing 快照与真实输入流程测试

## 5. 验证与文档

- [x] 5.1 生成并审阅双 profile 的四 App launch、fallback、进入/返回和 Menu 开合关键帧 PNG/GIF
- [x] 5.2 运行相关测试、完整 host suite、SDL3 desktop build/smoke/E2E 和 PlatformIO T-Embed compile
- [x] 5.3 审计 framebuffer/对象/firmware size、运行期分配、诊断、共享源码和 git diff --check
- [x] 5.4 更新验证文档，明确 30 FPS 真机闪烁、拖影、时长与人工视觉批准仍是 P8 边界
- [x] 5.5 生成四 App 完整 30 FPS 时间轴 PNG/GIF，逐帧复核 Cover、launch 与首屏无硬切
