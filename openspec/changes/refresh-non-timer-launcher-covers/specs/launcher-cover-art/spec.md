## ADDED Requirements

### Requirement: 非 Timer Cover 具有独立功能身份
MOTION、SETTINGS 与 ANIMATION GALLERY SHALL 各自以精确标题、一个 App 特有主 motif、一个宽支撑关系和至多两个语义 accent 构成 Cover；它们 SHALL 共享克制工业 1-bit 媒介语言但 MUST NOT 复制 TIMER 的斜盘与前景字母遮挡构图，也 MUST NOT 把相同的左侧标题、右侧物件版式重复为系列模板。标题位置、尺度关系与阅读方向 SHALL 针对每个 App 单独构图。

#### Scenario: 四张 Cover 在 Launcher 并列
- **WHEN** 用户在任一 profile 中依次浏览四个内置 App
- **THEN** 标题尺度、轮廓与灰阶纪律形成家族，空心牵引环、共同控制台、三姿态 figure 与倒计时盘分别清楚表达不同功能

### Requirement: MOTION 以空心牵引环表达弹性运动
MOTION SHALL 使用明亮背景、精确标题 `MOTION`、一个内部保持白色的大空心环、一条轨道和至多一个目标标记；MUST NOT 使用网格、粒子、多套箭头、测量刻度、靶心、第二仪器或底部机械轨。

#### Scenario: MOTION 硬阈值实尺寸审阅
- **WHEN** 候选以纯黑白 350×155 与 280×124 的 1× 尺寸显示
- **THEN** 完整标题、空心环和单一运动关系无需灰阶即可识别

### Requirement: SETTINGS 以共同控制台表达可调系统
SETTINGS SHALL 使用明亮背景、精确标题 `SETTINGS` 和一块共同基座上的旋钮、拨杆、滑块三个大剪影；共同基座 SHALL 具有连续、粗实、无需灰阶即可辨识的外轮廓，并以三至五个硬边离散平面表达体积。母图 MUST NOT 使用棚拍式连续光照、软阴影、环境遮蔽、镜面高光或照片级倒角，也 MUST NOT 使用螺丝、Wi-Fi 标志、标签、密集刻度、额外面板或设置列表。

#### Scenario: SETTINGS 硬阈值实尺寸审阅
- **WHEN** 候选以纯黑白双 profile 1× 尺寸显示
- **THEN** 标题与控制台首先形成完整插画身份，基座外轮廓连续可见，三个控制类别可区分且没有零件目录式噪声或照片转网点感

#### Scenario: SETTINGS 灰阶母图预审
- **WHEN** 候选尚未执行 ordered-dither 转换
- **THEN** 体积由少量边界明确的插画平面构成，不依赖连续高光、软阴影或灰阶照片质感来分离轮廓

### Requirement: ANIMATION GALLERY 以三姿态连续表演表达动画
ANIMATION GALLERY SHALL 使用暗色舞台、精确两行标题、同一 figure 的恰好三个姿态和一条 easing 或 spring 路径；MUST NOT 使用帧号、坐标轴、多条曲线、粒子、camera 标记或无关图标。

#### Scenario: GALLERY 硬阈值实尺寸审阅
- **WHEN** 候选以纯黑白双 profile 1× 尺寸显示
- **THEN** 两行标题完整可读，三个姿态沿一条路径形成清楚变化顺序且不依赖灰阶识别

### Requirement: 正式集成需要显式视觉批准
每张新 Cover 的 lossless 候选、pure 与双 profile reflective preview SHALL 在覆盖 source、PBM、header 或 golden 前获得用户明确批准；生成、转换、测试或 hash 成功 MUST NOT 等同于批准。

#### Scenario: 候选仍待批准
- **WHEN** 任一候选尚无用户明确批准
- **THEN** 正式资产和 approved golden 保持不变，候选仅存在于独立 review 目录

### Requirement: 批准 Cover 通过双 profile 管线
批准母图 SHALL 直接生成 350×155 与 280×124 的 PBM、texture-independent hard-threshold preview、ordered-dither pure/reflective preview、nearest-neighbor 4× preview 和 packed header，使用三至六个有意 tonal bands 与受控 ordered dither。转换 SHALL 显式清理连接近黑/近白的抗锯齿边缘及其 1 px guard band，同时 SHALL 保留宽阔结构灰面的 ordered dither；最终关键轮廓 SHALL 至少两像素且标题 counter 保持开放，运行时仍只等尺寸 blit。

#### Scenario: 重建批准资产
- **WHEN** 执行记录的生成和 check 命令
- **THEN** 两个 profile、硬阈值/1×/4×审阅产物与 headers 由同一命令生成，canonical PBM 与 headers 可复现且不存在二次缩放、运行时 PNG、缩放或分配

#### Scenario: Cover 沿 Launcher 轨道移动
- **WHEN** Cover 以一像素步进移动并经过 launch/return handoff
- **THEN** 内部像素保持静态且 ordered dither 无不可接受的闪烁或相位跳变

#### Scenario: 放大检查最终轮廓
- **WHEN** pure preview 以 nearest-neighbor 4× 放大审阅
- **THEN** 标题、baseline、counter 与主 silhouette 只包含规则连续的 1-bit 阶梯，不含贴边 Bayer 点、梳齿、针孔或灰色 patterned halo，且远离轮廓的结构灰面仍保留有意 dither
