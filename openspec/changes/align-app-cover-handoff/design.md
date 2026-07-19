## Context

`AppRuntime` 当前在 `open()` 时捕获 outgoing framebuffer，把 incoming 暂时复制为 outgoing；进度到 `0.5` 后执行唯一一次 `onExit -> onEnter`，随后才渲染 incoming App。默认策略是 0.32 秒百叶窗，因此 Launcher 的中央 Cover、App 身份与首屏之间没有视觉连续性，返回 Home 也只是同一转场配不同声音。

第一段参考视频为 460×452、5.75 秒的实拍。逐 50 ms 抽帧显示：约 `2.85 s` 开始启动，约 `3.00 s` Launcher chrome 已退场并留下中央内容，`3.15–3.20 s` 使用 1-bit 网点收束到黑场，约 `3.65 s` App 首屏出现。新增两段 App 视频进一步确认后半段并非统一系统模板：Music Box 在 chrome 退场后播放专属标题/扫描线序列再进入曲目 UI；另一 App 则让人物图直接延展为全屏动画。它们与本机锁定的 Playdate SDK 1.12.3 `Inside Playdate` 和 `Examples/Level 1-1/SystemAssets` 一致：静态 card 是 350×155；启动前由 Launcher 放在 400×240 的 `(25, 43, 350, 155)`；可选 `launchImages/` 是每 App 独立的 400×240 全屏序列。SDK 示例只作为语义与时序参考，不复制任何像素。

菜单参考视频放大到 20 FPS 后显示右侧面板不是刚性矩形平移：开合前缘是从上向下错开的斜线，内容在右边界与斜向前缘之间短暂横向压缩，形成薄片折入/被吸回的形变；面板约 0.1–0.2 秒完成运动，底层 App 始终冻结可见。当前 `SystemSurfaceCoordinator` 已有 0.16 秒线性 reveal，但 `closeMenu()` 会立即把 interactive surface 设为 `None` 并把 progress 清零，因此既没有关闭动画，也没有前缘形变。

项目既有调研已经固定并阅读 NobleEngine `93ffd6e` 的 Cover/Mix 中点换场、Taxman `5198188` 的 off-screen/dither 路径，以及 LVGL/Tweeny 的内存取舍。当前实现已为 transition 保留两张最大 12,000-byte framebuffer；新增第三张在 Sharp profile 会再增加 12 KB 常驻 RAM。现有 `add-continuous-launcher-navigation` 同时规定 Cover 是静态纯绘制身份，启动状态不得改变其像素，逐 App launch animation 必须使用独立契约。

## Goals / Non-Goals

**Goals:**

- 让默认 Launcher -> App 的启动节奏与参考视频一致：chrome 退场、Cover bridge、短黑场、首屏揭示。
- 允许每个 App 用独立、纯绘制、确定性的 launch sequence 表达自身身份，并让四个内置 App 的入场明显不同。
- 让 App -> Launcher 返回具有明确的“收回卡片”语义，并比进入更短、更直接。
- 让 System Menu 从右侧可逆地滑入和滑出，closing 期间保持冻结与输入所有权。
- 保持 Cover 像素静态、lifecycle 中点唯一、transition 输入冻结、双 profile 确定性和无运行期分配。
- 保留现有自定义 transition 的直接 source/destination 语义与已有策略集合。

**Non-Goals:**

- 不在本 change 引入运行时 PNG/PDI 加载、透明图层协议、外部逐帧文件格式或安装包资源发现；内置 App 先使用代码原生 launch renderer。
- 不复制参考视频、Playdate 示例图像、声音或品牌资产。
- 不通过运行时缩放 Cover 来伪造逐帧动画，也不修改 Launcher 导航、静态 Cover renderer 签名或 App 首屏内容。
- 不让 transition 期间目标 App 提前 update，也不为了视觉效果提前调用 `onEnter`。

## Decisions

### 1. 静态 Cover 与 App launch sequence 是两个独立契约

`App::renderLauncherCover()` 继续是静态、const、无进度输入的 App 身份。新增可选 `renderLaunchFrame(MonoCanvas&, float progress, const AppRenderContext&) const`，接受归一化进度与只读 render context 并绘制一张全屏 1-bit frame；它可以在 `onEnter` 之前调用，不得读取命令接口或改变 interactive/lifecycle 状态。只读 system snapshot 是必要输入，因为 Settings 首屏取决于 Motion/Sound/Connectivity 状态，若 launch 末帧假定默认设置，就会在 lifecycle 中点再次跳变。`AppCatalogView` 只读转发这项能力，不把可变 App 暴露给 Runtime。

四个内置 App 使用代码原生 renderer 建立明显不同的入场，但差异来自各自 Cover 与真实 UI 的关系，而不是插入独立海报：Clock 从计时 Cover 溶解到 Chrono 首屏，Motion 让同一球/轨道落入交互网格，Settings 让 Cover 的标题与控制器归并为实际左栏/设置行，Gallery 让封面标题与漂浮 frame 归并为 Gallery header/Easing 首屏。每个序列的首帧必须逐像素等于自身居中 Cover bridge，末帧必须逐像素等于同一 App 状态与 system snapshot 在 `onEnter` 后的首帧；没有实现 launch renderer 的 App 使用静态 Cover bridge 与受约束名称 fallback。

首轮实现曾在前 18% 直接绘制 Cover，随后整帧切到独立概念动画，末帧又与 App UI 无共享布局；这满足“每 App 不同”却不满足视觉连续。修订后使用固定相位 ordered-dither 将 Cover 像素单调交给真实首屏 preview，并复用 App 的首屏绘制 helper，消除两处硬切。30 FPS 相邻采样增加像素变化上限回归，避免未来重新引入整帧替换。

未把 `progress` 加到 Cover renderer，因为那会推翻已批准的静态像素契约；未让 launch renderer 调用普通 `render()`，因为目标 App 尚未 `onEnter`，普通 render 可能依赖 lifecycle 状态。未在首版定义外部逐帧文件格式，因为安装包、flash、解码与失败恢复尚未调研。

### 2. staged transition 在中点复用最后一张 launch/bridge frame

`Transition` 增加默认返回 `false` 的 capability query。现有 Cut/Blinds/Dip/Wipe/Iris/Dissolve 保持 direct model；新的 Launch Handoff 与 Return Handoff 返回 `true`。进入 App 时，Runtime 在中点前按当前 phase 把目标 App 的 launch frame 重绘到 incoming buffer；返回时 incoming 是当前 App 的静态 Cover bridge。中点执行：

```text
进入开始: outgoing = Launcher, incoming = launchFrame(0)
0.0..0.5: Launcher chrome -> launchFrame(0..1)
中点: Launcher.onExit -> App.onEnter
      outgoing = incoming  // 最后一张 launch frame
      incoming = App 首屏
0.5..1.0: launchFrame(1) -> App 首屏

返回开始: outgoing = App, incoming = static Cover bridge
0.0..0.5: App -> Cover bridge
中点: App.onExit -> Launcher.onEnter
      outgoing = incoming  // Cover bridge
      incoming = Launcher
0.5..1.0: Cover bridge -> Launcher chrome
```

两张 framebuffer 足够完成整条链路。未采用第三张 bridge buffer，因为它只简化实现却增加 6.8–12 KB 常驻 RAM；未让 Runtime 根据具体 transition 类型做指针比较，因为用户构造的同类策略也应正确声明行为。

### 3. 进入与返回使用不同的系统策略和时长

默认构造的 Runtime 使用方向路由，而显式注入或 `setTransition()` 后继续使用调用方策略与时长：

- Launcher -> App：约 0.8 秒。前半播放 App 独立 launch sequence 并移除 Launcher chrome，后半从稳定末帧揭示 App 首屏。
- App -> Launcher：约 0.44 秒。App 直接 dither 收束到自身静态 Cover bridge，中点后恢复 Launcher chrome；不倒放 App 的 launch sequence。
- 非 Home App -> 非 Home App：保留直接 Dip，避免臆造 Home/card 空间语义。

这不是简单倒放：启动需要 anticipation 和 App 自我介绍，返回则应响应更快并明确落回原卡片。未统一使用较长时长，因为系统返回会显得迟钝；未继续使用 0.32 秒，因为三段参考视频都显示 App 专属 sequence 是被感知到的完整阶段，而不是一次短 crossfade。

返回逐帧审阅进一步发现两个离散问题：`inOutQuad` 会把 App → Cover 的 ordered-dither threshold 挤在中间两帧，双 profile 峰值达到约 21–23%；Sharp 的 155 px 高 Cover 若使用整数向下居中则落在 `y=42`，而 Launcher/Playdate card 契约位于 `y=43`。修订后前半段使用匀速 threshold 交接，把 30 FPS 相邻变化限制在 16% 内；Cover bridge 与 launch renderer 对奇数剩余空间统一向下视觉居中（整数坐标向上取整），使 400×240 内容矩形精确保持 `(25,43,350,155)`，中点后 Cover 区域不再移动或重绘。

### 4. 1-bit ordered dither 负责层间混合，App renderer 负责内容运动

Handoff 不引入灰阶或 alpha。系统层按现有 8×8 ordered threshold table 逐像素选择 Launcher、launch frame、Cover bridge 或 App 首屏；App renderer 在自己的全屏 frame 内用整数几何、裁剪和固定 pattern 表达独立运动。dither 相位固定在屏幕坐标，避免 pattern crawling；端点精确复制 framebuffer。

未采用 nearest-neighbor 运行时缩放 Cover：它在 1-bit 细线 Cover 上容易产生跳线和闪烁，也与当前“两个 profile 均离线生成 Cover”的决策冲突。App 可以重画同一视觉元素，但不得把已二值化 Cover bitmap 连续缩放当作 launch sequence。

### 5. System Menu 使用显式 opening/open/closing 状态

`SystemSurfaceCoordinator` 在 close intent 后不立即释放 `interactive_`，而进入 `closing`：reveal progress 按约 0.16 秒递减，面板仍渲染并冻结 App，所有输入继续被 surface 消费；progress 到 0 后才设为 `None`。打开沿用约 0.16 秒但使用 ease-out，关闭使用 ease-in 向右离屏，与参考视频的快速开合一致。

渲染不把 panel 当作刚性矩形。Runtime 复用 transition 的闲置 incoming framebuffer 先在静止坐标绘制完整 Menu，再逐 scanline 合成：上方行比下方行更早进入、也更早退出；每行把完整 panel 宽度重新采样到“右边界—当前斜向前缘”的可见宽度，从而产生受限横向压缩。所有映射使用整数 nearest-neighbor、固定最大循环和 caller-owned scratch，不引入第三张 framebuffer 或 heap。未采用自由曲面/透视浮点 mesh，因为 1-bit 输出与 ESP32 成本不值得；未只裁剪三角形，因为那能做斜前缘，却缺少视频中明显的内容挤压。

`menuActive()` 表示 surface 仍占据 composition slot（包括 closing），另以 internal flag 禁止 closing 时导航或重复 action。Closed sound/diagnostic 在用户动作被接受时发出一次，不等动画结束重复发出。Home action先关闭 Menu composition，再启动 App return transition，避免两个 active system animations 争用同一冻结 framebuffer。

未用负 dt 或简单倒放 opening 状态，因为关闭期间的输入所有权、声音时点和 Home action 都需要显式语义；未让底层 App 在滑出时恢复 update，因为参考视频中底层内容保持冻结，提前恢复会造成面板下画面跳动。

### 6. Reduced Motion 保留因果关系并压缩非必要运动

两种 motion profile 都必须看见 Launcher、App launch identity 和 destination 的因果关系。Reduced Motion 不瞬切，而缩短 App sequence 的非必要等待、降低内容位移，并缩短 Menu panel travel 的时间；系统 dither 与静态关键帧仍保留。profile 在一次 App transition 或 Menu open/close 开始时锁定，避免中途设置变化造成时间轴跳变。

## Risks / Trade-offs

- [新增 launch renderer 是长期 App 公共契约] → API 保持 const、单一归一化进度、无资源所有权和默认 `false`；先用内置代码原生序列验证，外部资源协议另立 change。
- [0.8 秒默认启动比旧 0.32 秒慢] → 返回单独压到约 0.44 秒；用 30 FPS 关键帧和真实点击流程审阅，若体感拖沓只调系统常量，不改 lifecycle/bridge 契约。
- [中点复制最大 framebuffer 有固定 CPU 成本] → 复制量最多 12 KB、每次切换仅一次；用 host benchmark/真机 frame-time 观察，且比常驻第三 buffer 更符合内存预算。
- [fallback bridge 与 Launcher fallback 若分别实现可能漂移] → 抽取或共享同一受约束布局 helper，并用无 Cover App 的双 profile snapshot 锁定。
- [自定义 transition 可能误报 staged capability] → staged 策略测试覆盖 pre/post midpoint source 内容和生命周期；默认 false 保证旧实现安全。
- [closing Menu 会延后 App 恢复约 0.16 秒，scanline 压缩可能让文字短暂难读] → 形变只存在于快速开合段，open 稳态像素不变；输入与冻结契约显式覆盖，Home action不等待滑出而直接进入 return transition。
- [Memory LCD 上 dither 可能出现拖影] → host snapshot 只证明像素确定性；P8 必须在真实硬件以 30 FPS 检查闪烁、拖影和黑场长度，不把模拟器观感当批准结论。

## Migration Plan

1. 先增加纯 launch renderer、staged transition、菜单 closing、进入/返回关键帧和 midpoint buffer 的失败测试。
2. 实现 launch-frame/bridge capture 与中点两 buffer 复用，再加入四个内置序列、方向路由和 motion-profile 时序。
3. 实现 Menu opening/closing state 与 easing，更新输入冻结和 sound intent 测试。
4. 更新双 profile snapshot、headless/desktop E2E、默认时长相关断言和文档。
5. 运行完整 host、desktop、firmware-compatible build 与内存/诊断审计；真机观感保留为唯一人工批准项。
6. 若需回滚，默认路由恢复为 Venetian Blinds、Menu close 恢复立即释放；可选 renderer 与 staged query 可保留为未默认启用的兼容能力，不涉及持久数据迁移。

## Open Questions

- 外部逐 App launch frame/animation 的资源来源、flash 预算、第三方 App 打包与加载失败降级，继续作为后续独立 change；本次 const renderer 只定义行为，不预留未经验证的文件 API。
- 真机审阅后，Normal 进入时长和黑场比例可在不改变协议的情况下微调；自动化先锁定阶段顺序与端点，不把尚未人工批准的毫秒值永久写成跨版本公共契约。
