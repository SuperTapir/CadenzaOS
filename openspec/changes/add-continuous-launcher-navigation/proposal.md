## Why

当前 Launcher 在旋钮改变选择时会立即替换中央卡片内容；虽然代码维护了平滑位置，但渲染没有消费该状态，循环边界也没有最短路径语义。这使系统最频繁的导航入口缺少空间连续性，并与项目已有的动画、reduced-motion 和 Playdate 设计调研结论脱节。

## What Changes

- 将 Launcher 改为可中断、循环且连续的卡片轨道，导航期间同时呈现当前项与相邻项，并在循环边界沿单卡最短路径移动。
- 在 Settings 增加 `LAUNCHER: VERTICAL | HORIZONTAL`，切换后通过受权限约束的系统设置快照立即传播到 Launcher；本 change 保持现有会话级设置生命周期，不引入通用持久化存储。
- 优化 Launcher 交互：快速连续旋转连续 retarget、移动中点击打开逻辑选中项、相邻卡片提供方向预告、页脚去除重复的前后 App 名称，并保留稳定的选择指示。
- 为 App 增加固定容量、无分配的 Launcher Cover 绘制契约；Clock、Motion、Settings 和 Gallery 各自使用经人工确认、离线裁切并转换为 1-bit 的原创静态内容场景，没有 Cover 的 App 使用受约束名称 fallback。
- Cover 明确定义为静态、不可交互的视觉身份；选中、按下、长按、启动和 App lifecycle 状态均不得改变 Cover 像素。未来若需要启动动画，应使用独立资源与契约，不复用或变异 Cover。
- 重做 Launcher 视觉层级：应用内容成为卡片主体，系统 chrome 收敛为轻量轮廓和简洁标题栏，移除笨重双层大框、通用巨字、重复页码 footer 和抢眼移动纹理。
- Normal motion 使用轻微受控 overshoot，Reduced Motion 取消 overshoot 但保留连续导航反馈。
- 保持背景纹理在屏幕坐标中稳定，避免卡片移动引发高频 1-bit 相位闪烁；对 320×170 与 400×240 profile 定义一致的比例布局和裁剪边界。
- 增加连续中间帧、快速输入、循环边界、横纵布局、设置传播、点击语义、reduced-motion、文本边界和双分辨率回归验证。

## Capabilities

### New Capabilities

- `launcher-card-navigation`: 定义连续循环卡片轨道、横纵布局、输入 retarget、打开语义、静态 App Cover/fallback、motion profile 和 Settings 方向偏好的产品契约。

### Modified Capabilities

- `bounded-text-layout`: Launcher 动态 App 名称从中央主卡片与 footer 邻项标签改为每张可见卡片各自受约束布局，且不得越过轨道裁剪区域或导航指示器。
- `runtime-verification`: Launcher 验证从静态选择终态扩展到双 profile 的方向布局、动画中间帧、循环与快速交互状态。

## Impact

- `cadenza_core` 的 App/目录只读视图增加无交互状态输入的 Launcher Cover 绘制契约；系统快照与设置命令增加 Launcher 布局方向这一小型公共枚举和会话态设置值。
- `cadenza_system` 负责校验并提交该设置；Settings 获得新行并保持现有 `SettingsWrite` 权限边界。
- `cadenza_apps` 的 Launcher 状态、更新和绘制改为无界逻辑轨道位置与轴向布局；四个内置 App 增加静态 1-bit Cover renderer 与构建期生成的内嵌位图。App catalog 的注册和打开语义保持兼容，未实现 Cover 的第三方/测试 App 自动走 fallback。
- App、system service、headless/desktop、快照与 firmware-compatible 测试需要同步更新；图片转换仅使用仓库工具离线执行，不增加运行时第三方依赖、动态分配、可安装 App 资源协议或持久化子系统。
