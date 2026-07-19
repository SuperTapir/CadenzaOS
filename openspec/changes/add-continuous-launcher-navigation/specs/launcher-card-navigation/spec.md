## ADDED Requirements

### Requirement: Launcher 使用连续循环卡片轨道
Launcher SHALL 同时呈现当前 App 卡片和沿导航轴相邻的 App 卡片，并 SHALL 通过连续视觉位置在选择之间移动，而不是在输入帧瞬时替换中央内容。卡片轨道 SHALL 循环访问所有可见 App，且不得分配运行时堆内存。

#### Scenario: 单步选择产生中间帧
- **WHEN** 用户从静止状态旋转一个选择步并在动画完成前渲染
- **THEN** 旧卡片与新卡片都按同一连续轨道处于中间位置，且新卡片尚未瞬时吸附到中央

#### Scenario: 末项前进到首项
- **WHEN** 当前选择是最后一个 Launcher App 且用户向前旋转一步
- **THEN** 轨道沿输入方向移动一个卡片间距并选择第一个 App，而不是反向跨过其余所有 App

#### Scenario: 首项后退到末项
- **WHEN** 当前选择是第一个 Launcher App 且用户向后旋转一步
- **THEN** 轨道沿输入方向移动一个卡片间距并选择最后一个 App

### Requirement: App Cover 是内容优先的可选绘制契约
App SHALL 能通过 const、无分配的 Cover renderer 在 Launcher 分配的正尺寸 bounds 内表达自身视觉身份。内置位图 Cover SHALL 由固定输入离线转换为 packed 1-bit 数据，并为支持的 framebuffer profile 提供完整、等宽高比尺寸；在运行时只执行整图等尺寸 blit，不得裁切当前 Cover 内容、加载文件、缩放或分配临时 framebuffer。Cover renderer SHALL NOT 接收选中、按下、启动、时间或 lifecycle 状态，且不得观察或修改 App lifecycle 状态、越过 bounds 或提交无效几何。Launcher SHALL 在 renderer 缺失或返回未绘制时提供包含受约束 App 名称的确定 fallback。

#### Scenario: 内置 App 绘制原创 Cover
- **WHEN** Launcher 在任一支持 profile 下呈现 Clock、Motion、Settings 或 Gallery
- **THEN** 对应 App 绘制可与其他内置 App 区分的、经审阅的原创 1-bit 内容场景，且系统不以统一巨字黑框替代该场景

#### Scenario: 内置 Cover 资源可复现
- **WHEN** 对固定源图重复执行仓库记录的裁切、阈值和 packed bitmap 转换流程
- **THEN** 生成的尺寸、字节和 C++ header 与已提交资源一致

#### Scenario: App 没有 Cover renderer
- **WHEN** 可见 App 使用默认 Cover 实现或明确返回未绘制
- **THEN** Launcher 在同一卡片 bounds 内绘制确定 fallback 和受约束 App 名称，导航与打开仍完整可用

#### Scenario: Cover renderer 受 bounds 约束
- **WHEN** 每个内置 Cover 在 320×170 与 400×240 的完整、部分可见卡片 bounds 中绘制
- **THEN** framebuffer 外和 bounds 外像素保持不变，且不产生 InvalidGeometry、ClippedGeometry 或 FullyClipped 诊断

### Requirement: Cover 在交互与生命周期中保持静态
同一 App、profile 与 bounds 的 Cover SHALL 在未选中、选中、按钮按下、长按、启动以及 App lifecycle 状态变化前后保持逐像素一致。Launcher 导航 SHALL 只改变整张 Cover 的轨道位置和可见裁剪区域，不得重新布局或驱动 Cover 内部元素。未来启动动画 SHALL 使用独立资源与契约，不得通过变异 Cover 实现。

#### Scenario: 当前选择改变
- **WHEN** turn 使逻辑选择从一个 App 变为另一个 App
- **THEN** 两张 Cover 的内部像素保持不变，仅沿连续轨道改变整体位置和可见裁剪区域

#### Scenario: 用户按住打开按钮
- **WHEN** Launcher 收到 button pressed 且尚未释放
- **THEN** 按下、保持与释放前后的当前 Cover 像素完全相同

#### Scenario: 有效点击启动 App
- **WHEN** Launcher 点击成功请求打开当前逻辑选择
- **THEN** outgoing Launcher 捕获中的 Cover 与点击前逐像素相同，随后沿现有 Runtime transition 进入 App

#### Scenario: App 内部状态已经变化
- **WHEN** Clock 时间、Motion 位置或其他 App lifecycle 状态在运行期间发生变化后再次绘制其 Cover
- **THEN** Cover 与相同 bounds 下的初始 Cover 逐像素相同

### Requirement: Launcher 系统 chrome 保持克制
Launcher SHALL 让 App Cover 占据主要视觉面积，并 SHALL 仅使用轻量、稳定的系统标题与单层卡片边界。相邻 Cover SHALL 直接表达顺序与方向；Launcher SHALL NOT 同时显示重复的前后名称、页码 footer、笨重双层外框或随时间漂移的高对比背景纹理。

#### Scenario: settled Launcher 呈现多个 App
- **WHEN** Launcher 在任一方向稳定显示当前和相邻 Cover
- **THEN** 当前 Cover 是画面主焦点，相邻 Cover 形成空间预告，且不存在重复 footer 导航信息

### Requirement: Launcher 导航可被连续输入中断并 retarget
Launcher SHALL 在前一次选择移动尚未完成时接受新的 turn delta，立即更新逻辑目标并从当前视觉位置连续追赶新目标。一个输入帧 SHALL 保留完整 turn delta，同时最多产生一个语义 Navigate cue。

#### Scenario: 动画中再次旋转
- **WHEN** 用户在轨道仍移动时再次沿相同或相反方向旋转
- **THEN** 视觉位置从当前值连续改变、没有状态跳变，最终选择对应所有累计 turn delta

#### Scenario: 单帧包含多个选择步
- **WHEN** 一个 InputFrame 的 turn 绝对值大于一
- **THEN** 逻辑选择应用全部步数且声音服务只收到一个 Launcher 导航 cue

### Requirement: 移动中打开最新逻辑选择
Launcher SHALL 在每次有效 turn 时立即更新逻辑选择。点击 SHALL 打开该逻辑选择对应的 App，不得等待视觉轨道 settled，也不得打开移动前的旧 App。

#### Scenario: 旋转后立即点击
- **WHEN** 用户旋转到下一 App 并在轨道到达中央前点击
- **THEN** Runtime 开始打开下一 App

### Requirement: Launcher 支持横向和纵向布局
系统 SHALL 提供 `Vertical` 与 `Horizontal` 两种 Launcher orientation。两种方向 SHALL 共享相同的目录顺序、循环、输入、声音、打开和导航指示语义；仅卡片运动轴、尺寸与相邻项可见边改变。

#### Scenario: 纵向布局导航
- **WHEN** orientation 为 Vertical 且用户改变选择
- **THEN** 卡片沿 y 轴连续移动，上一项和下一项从轨道 viewport 的上边与下边提供空间预告

#### Scenario: 横向布局导航
- **WHEN** orientation 为 Horizontal 且用户改变选择
- **THEN** 卡片沿 x 轴连续移动，上一项和下一项从轨道 viewport 的左边与右边提供空间预告

#### Scenario: 切换方向保留选择
- **WHEN** Settings 从 Vertical 切换为 Horizontal 或反向切换
- **THEN** Launcher 保持相同逻辑 App 选择，并以新方向绘制稳定轨道

### Requirement: Settings 控制 Launcher 方向
Settings SHALL 显示可发现的 Launcher orientation 行，并 SHALL 通过要求 `SettingsWrite` capability 的系统命令循环切换 Vertical 与 Horizontal。提交成功后的 render snapshot SHALL 在同帧反映新方向；设置生命周期 SHALL 与当前会话级 Motion 和 Sound 设置一致。

#### Scenario: Settings 切换 Launcher 方向
- **WHEN** 授权 Settings App 在 Launcher 行确认
- **THEN** 系统快照从 Vertical 变为 Horizontal 并播放成功切换反馈

#### Scenario: 未授权 App 尝试修改方向
- **WHEN** 不具备 SettingsWrite capability 的 App 提交方向命令
- **THEN** 系统拒绝命令、记录 capability rejection，且 orientation 保持不变

#### Scenario: 主机重新建立服务
- **WHEN** 创建新的 SystemServiceHost 会话
- **THEN** Launcher orientation 使用默认 Vertical，不依赖前一会话状态

### Requirement: Motion profile 保留连续导航并精确停靠
Normal 与 Reduced Motion SHALL 都使用无 overshoot 的单调 ease-out，并 SHALL 精确停靠目标位置。Normal SHALL 使用 250 ms 的快速响应节奏；Reduced Motion SHALL 缩短运动时间但保留连续位移、方向信息和相邻卡片，不得降级为瞬切。

#### Scenario: Normal Motion 下改变选择
- **WHEN** motion profile 为 Normal 且用户从静止状态旋转一个选择步
- **THEN** 轨道通过多个确定性中间帧单调接近目标，在 250 ms 内精确停靠且不越过目标位置后回拉

#### Scenario: Reduced Motion 下改变选择
- **WHEN** motion profile 为 Reduced 且用户旋转一个选择步
- **THEN** 轨道通过多个确定性中间帧单调接近目标、运动时间短于 Normal 且不越过目标位置

#### Scenario: 移动中切换 Motion profile
- **WHEN** motion profile 在 Launcher 尚未 settled 时改变
- **THEN** 视觉位置从当前值继续且没有可见跳变，后续运动遵守新 profile

### Requirement: Launcher 运动避免无关的 1-bit 背景相位变化
Launcher 的装饰背景 SHALL 固定在屏幕坐标中，不得仅因时间推进而漂移。卡片移动 SHALL 不携带会逐像素反相的高频抖动纹理，并 SHALL 在静止 settled 后产生确定帧。

#### Scenario: 静止 Launcher 随时间更新
- **WHEN** 没有输入、选择和设置变化，仅以不同 dt 更新已经 settled 的 Launcher
- **THEN** framebuffer 内容保持相同

### Requirement: 空目录和长时间运行保持安全
Launcher SHALL 在目录没有可见 App 时显示受约束的空状态且忽略打开；长期循环导航 SHALL 通过等价重基避免轨道整数溢出或浮点精度持续下降。

#### Scenario: 空 Launcher 接收输入
- **WHEN** catalog 没有可见 App 且 Launcher 接收 turn 或 click
- **THEN** 不发生无效 App 打开、除零、越界访问或非有限视觉状态

#### Scenario: 轨道位置达到重基阈值
- **WHEN** settled 的目标位置绝对值达到实现声明的安全重基阈值
- **THEN** 目标与视觉位置被移动到等价的小坐标，当前 App 和 framebuffer 保持不变
