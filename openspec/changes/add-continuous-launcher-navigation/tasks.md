## 1. 系统设置契约

- [x] 1.1 先增加 LauncherOrientation 默认值、命令合法性、SettingsWrite 权限拒绝与同帧 snapshot 提交的失败测试
- [x] 1.2 在 core SystemSnapshot/SystemCommand 和 SystemServiceHost 中实现会话级 Launcher orientation 设置
- [x] 1.3 增加 Settings Launcher 行、方向文案与即时切换测试，并在双 profile 验证布局无越界

## 2. 连续轨道状态与交互

- [x] 2.1 先增加单步中间态、末首最短环绕、多步/反向 retarget、移动中点击和空目录的失败测试
- [x] 2.2 用无界逻辑目标、连续视觉位置和安全重基替换 Launcher 未消费的 position 状态
- [x] 2.3 实现 Normal 弹簧与 Reduced Motion 单调收敛，并增加 profile 中途切换、无 overshoot 和 settled 确定性测试

## 3. App Cover 契约与原创内容

- [x] 3.1 先增加静态 Cover 默认 fallback、const 纯绘制、交互/lifecycle 像素不变和 bounds/诊断的失败测试
- [x] 3.2 在 App 与 AppCatalogView 中实现不接收交互状态的窄 Cover renderer 契约和默认未绘制行为，不暴露可变 App 指针
- [x] 3.3 为 Clock、Motion、Settings、Gallery 实现可区分的原创代码原生 Cover，并验证双 profile 无越界/无分配
- [x] 3.4 验证按钮按下、长按和 Runtime outgoing 启动捕获不会改变 Cover 像素
- [x] 3.5 将确认后的 Clock、Motion、Settings、Gallery 插图离线转换为 350×155 与无裁切等比 280×124 的 1-bit packed bitmap，接入整图 renderer，并增加转换可复现性与 flash 体积验证
- [x] 3.6 修正 T-Embed Cover 的二次缩放损失：两个 profile 均直接从高分辨率母图生成目标尺寸，并以像素级来源检查锁定转换参数

## 4. 横纵卡片轨道与视觉重构

- [x] 4.1 实现共享轴向 viewport、固定容量可见 slot、部分 Cover 显式裁剪与无 Cover 的受约束标题 fallback
- [x] 4.2 在插图方向确认后收敛单层轻量 card chrome 与最密屏幕坐标水平纹背景，重复 footer/页码和笨重双框已删除，固定比例 Cover 视觉已批准
- [x] 4.3 增加 Vertical/Horizontal、长标题、相邻 Cover 可见、静止时间稳定和零 Invalid/Clipped geometry 诊断测试

## 5. 快照、流程与文档

- [x] 5.1 为两个 framebuffer profile 增加两种 settled orientation、确定性动画中间帧、每个内置 Cover、fallback 与按下/启动像素不变代表快照并逐图审阅
- [x] 5.2 扩展 headless/desktop 真实流程，覆盖 Settings 切换方向、返回 Launcher、快速选择、按下 Cover 不变并在移动中打开
- [x] 5.3 更新开发与验证文档，记录 Launcher/Cover 操作、会话级设置边界、Playdate 参考采用/不采用决策和 30 FPS/1-bit 真机验收步骤
- [x] 5.4 从 Playdate 1.12.3 参考图锁定 reflective ink/paper 色板，在 SDL 呈现边界实现默认暖灰与显式 pure 模式，并验证不改变 canonical 1-bit 输出
- [x] 5.5 为 SDL 窗口启用 macOS 高像素密度 backing，启动时报告逻辑尺寸、实际像素尺寸与 density，并保持最近邻整数放大

## 6. 完整验证

- [x] 6.1 运行 OpenSpec validate、相关单测与完整 host test suite
- [x] 6.2 完成 SDL3 desktop 双 profile 全新进程 smoke/E2E、诊断日志审计、PlatformIO T-Embed 编译和共享源码审计
- [x] 6.3 运行 git diff --check，逐项审计规格、任务与测试证据，并记录只能在真机执行的闪烁/拖影验收边界
