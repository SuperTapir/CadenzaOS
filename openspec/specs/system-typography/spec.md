# System Typography Specification

## Purpose
定义系统界面的字体来源、语义字号角色、双 profile 密度、度量缓存、布局边界、资源回退策略、构建约束与可读性门禁。

## Requirements

### Requirement: 系统字体必须使用统一的语义角色
系统 SHALL 提供 `Hero`、`Title`、`Body`、`Menu`、`Compact`、`Caption`、`Footer` 七个 typography role。产品 App 和 system surface SHALL 只选择语义角色，而不得持有裸字体数据指针或根据设备选择字号。同一个 role 在任何 viewport 中 SHALL 表达相同的信息层级。

#### Scenario: 常规设置页声明文字层级
- **WHEN** Settings 绘制页面标题、主要列表文字和右侧辅助值
- **THEN** 三者分别通过 `Title`、`Menu` 和 `Caption` role 获得可审计且一致的字体资源

### Requirement: Typography 必须在 Canvas setup 时按 viewport 集中解析
`MonoCanvas` SHALL 在构造或 setup 时根据 framebuffer viewport 一次性解析 typography density，并缓存七个 role 对应的字体 descriptor。resolver SHALL 只读取尺寸，不得读取 framebuffer profile、平台或设备型号；App 绘制期间不得出现设备 typography 分支。常规 viewport SHALL 解析为 `Hero=24 Medium`、`Title=24 Medium`、`Body=20 Medium`、`Menu=11 Bold`、`Compact=11 Bold`、`Caption=11 Medium`、`Footer=9 Mono Condensed`；紧凑 viewport SHALL 解析为 `Hero=24 Medium`、`Title=20 Medium`、`Body=20 Medium`、`Menu=11 Bold`、`Compact=10 Bold`、`Caption=11 Medium`、`Footer=9 Mono Condensed`。

#### Scenario: 相同语义在不同 viewport 解析
- **WHEN** 在 320×170 和 400×240 canvas 上以相同 role、字符串和 scale 测量文本
- **THEN** 两者使用相同 role 与测量路径，但 320×170 的 `Title`/`Compact` 分别采用 20 Medium/10 Bold，400×240 分别采用 24 Medium/11 Bold，且选择路径不包含 profile 或设备条件

#### Scenario: 同一 viewport 的解析保持确定
- **WHEN** 两个 Canvas 使用相同 viewport 构造并重复测量和绘制
- **THEN** 它们缓存相同 descriptor，产生完全相同的 metrics 与 glyph pixels，绘制期间不重新计算 density

### Requirement: 系统字号必须使用原生 bitmap
`Hero`、`Title`、`Body`、`Menu`、`Compact`、`Caption` 和 `Footer` SHALL 默认以各自原生 scale 1 光栅化；产品代码不得通过放大较小系统字体模拟另一个 typography role。

#### Scenario: 绘制主要正文
- **WHEN** App 绘制 `Body` 文本
- **THEN** glyph 来自 Roobert 20 Medium 原生 bitmap，而不是 Roobert 11 或 legacy 6×10 的 nearest-neighbor 放大

### Requirement: 导航菜单必须使用可读的 Menu 语义
System Menu 与 Settings 的可操作行 SHALL 使用 `Menu=11 Bold`，不得在紧凑 viewport 降为 10 Bold。行容器 SHALL 用字体真实高度分配 padding；Settings 未选中行 SHALL 通过留白分组而不是连续描边框，选中行才显示圆角反色背景。Settings 的 Motion 与 Sound SHALL 复用 System Menu 的 toggle 与 volume indicator，包括选中态反色语义。

#### Scenario: T-Embed 绘制 System Menu
- **WHEN** 320×170 Canvas 绘制导航菜单项
- **THEN** 菜单文字使用 11 Bold，30 px row 中保留均衡 padding，且 selected background 覆盖完整 item bounds

### Requirement: App 底部操作栏必须使用统一 Footer 语义
Timer、Motion、Settings、Gallery 和其他内置 App 的底部操作提示 SHALL 使用 `Footer`，不得复用较大的 `Caption` 或在 App 内单独选择设备字号。`Footer` SHALL 保持 14 px 原生行框和较轻的 Roobert 9 Mono Condensed 字形。

#### Scenario: T-Embed 绘制 App 操作栏
- **WHEN** 任一内置 App 在 320×170 Canvas 绘制底部操作提示
- **THEN** 提示使用 `Footer` 的原生 9 Mono Condensed descriptor，且不改变 Timer 数字或页面 Caption 的 renderer

### Requirement: 业务 Display typography 必须与系统 role 分层
Timer 数字、Launcher Cover 字图、动画内容和其他明确的业务展示字形 SHALL 保留专用 renderer 或 asset，不得因迁移系统字体而被统一替换成 Roobert role。它们 SHALL 继续接受确定性、clipping、license 和 snapshot 验证。

#### Scenario: Timer 绘制倒计时数字
- **WHEN** Timer 绘制主倒计时数字并同时绘制系统说明文字
- **THEN** 数字继续来自专用 numeral atlas，说明文字通过语义 role 解析，两条渲染路径互不替换

#### Scenario: Timer 绘制主告警与确认按钮
- **WHEN** Timer alert 在任一 viewport 绘制 `TIME UP` 和确认按钮
- **THEN** 主告警通过 `Hero` 保持 24 Medium，按钮宽度按 `Compact` 的真实 metrics 加 padding 后受卡片内框约束，不发生文字 overflow

### Requirement: 字体来源与修改必须可审计
每个 Roobert 源文件 SHALL 锁定版本和 SHA-256，记录 CC BY 4.0、适当署名、转换方式、打包 glyph 集与生成资源大小。

#### Scenario: 审核字体 manifest
- **WHEN** 维护者检查所有编入固件的 typography 资源
- **THEN** 每个资源都能追溯到 Playdate SDK 3.0.6 源文件、校验和、许可证与确定性生成命令
