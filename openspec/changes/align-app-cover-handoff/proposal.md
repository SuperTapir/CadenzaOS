## Why

当前 AppRuntime 默认以百叶窗直接替换整个 Launcher 与 App 画面，既没有延续已选 Cover 的视觉身份，也无法承载每款 App 不同的入场表达；System Menu 虽会滑入，关闭时却瞬间消失。三段参考视频共同展示了更清楚的职责划分：系统先让 Launcher chrome 退场并留下 App 内容身份，每个 App 再播放独立 launch sequence；菜单则由系统 surface 自己完成可逆的侧滑开合。Cadenza 需要在不改变静态 Cover 像素、不增加第三张全屏 framebuffer 的前提下建立这套动效语言。

## What Changes

- 将默认 App 切换改为确定性的两段式 handoff：`Launcher -> App launch sequence -> App first frame`，中点仍是唯一 lifecycle 交接点。
- 为 App 增加可选、const、无 lifecycle side effect 的 launch-frame renderer；Clock、Motion、Settings 与 Gallery 提供彼此不同的代码原生入场序列，没有实现的 App 走静态 Cover bridge fallback。
- 进入 App 时，Launcher 的选中 Cover 在原位保留，轨道与 card chrome 先退场；App 专属序列的首帧从同一 Cover 身份出发，末帧再通过 1-bit dither 衔接 App 首屏。
- 返回 Launcher 时使用可理解的反向交互：App 首屏先收束为自身静态 Cover bridge，再恢复 Launcher 轨道与 chrome，而不是简单倒放或再次播放百叶窗。
- 静态 Cover 契约继续不接收 pressed、launch、time 或 lifecycle 状态；launch renderer 是独立能力，不得变异 Cover，也不得提前激活 App lifecycle。
- Runtime 继续只持有两张 transition framebuffer：中点将 bridge 从 incoming buffer 搬到 outgoing buffer，再在 incoming buffer 渲染已进入的目标 App。
- 自定义 transition 仍保留原有单段 source/destination 语义；只有显式声明 staged handoff 的策略使用 bridge buffer 协议。
- System Menu 增加显式 closing 状态、右侧面板的 ease-out/ease-in 侧滑与短暂斜向压缩变形；关闭动画期间继续冻结 App、吞掉输入，并在面板完全离屏后释放 surface。
- 为 Normal/Reduced Motion、双 profile、中点连续性、端点精确性、输入冻结和 lifecycle 顺序增加自动化验证，并保留 Memory LCD 真机观感验收。

## Capabilities

### New Capabilities

- `app-cover-handoff`: 定义静态 Cover bridge 的进入/返回语义、两段式合成、中点 buffer/lifecycle 契约、fallback 与 motion-profile 行为。
- `app-launch-sequence`: 定义每 App 可选的纯绘制 launch-frame 契约、内置 App 差异化序列、首尾衔接与无实现 fallback。
- `system-menu-motion`: 定义 System Menu 的可逆侧滑开合、closing 输入所有权、冻结与完成语义。

### Modified Capabilities

- `animation-presentation`: 将默认 Runtime transition 从单段百叶窗改为可选的 staged Cover handoff，同时保留现有 transition set 与自定义策略。
- `runtime-verification`: 增加双 profile 的进入/返回关键帧、bridge 连续性、生命周期与固定内存回归要求。

## Impact

- `cadenza_core` 的 `App`/catalog 只读 launch renderer、`Transition` 能力查询、`AppRuntime` capture/swap、默认时长和 `SystemSurfaceCoordinator` closing 状态会调整；App lifecycle 与注册方式保持兼容。
- `cadenza_apps` 为四个内置 App 增加代码原生、无分配的差异化 launch renderer，但不修改已批准 Cover 像素；Launcher 布局只作为首帧对齐的既有视觉契约被复用。
- transition/runtime/launcher/system-surface/headless/desktop 测试与快照需要更新；固件仍保持 C++17、无运行期分配、无平台图形依赖。
- 本 change 不复制参考视频或 Playdate SDK 美术资产，也不引入外部 App launch-image 文件协议；第三方可安装 App 的资源打包仍留待独立调研。
