## ADDED Requirements

### Requirement: 只读参考基线
系统 SHALL 保存原嘉立创 EDA 工程、PCB、原理图、库、外壳、附件、页面证据、许可证信息和文件哈希，并 SHALL 在独立派生目录中进行所有修改。

#### Scenario: 创建派生工程
- **WHEN** 开始修改屏幕、输入、USB、PCB 或外壳
- **THEN** 原参考文件保持哈希不变，所有新文件写入独立的 Cadenza 派生目录

### Requirement: 转换完整性门禁
KiCad 派生工程 SHALL 在功能修改前与嘉立创 V2 原工程比较器件、网络、层、孔、板框、关键坐标、封装和铺铜；未解释的差异 SHALL 阻止继续布板。

#### Scenario: EasyEDA Pro 导入 KiCad
- **WHEN** V2 `.epro` 首次导入并保存为 KiCad 工程
- **THEN** 审计报告记录参考和导入后的组件数、网络数、铜层数、过孔数、安装孔、板框尺寸与关键器件坐标，并标记每个差异

#### Scenario: 转换不满足要求
- **WHEN** 导入造成关键网络断开、板框变化、封装丢失或不可恢复的铺铜变化
- **THEN** 实施 SHALL 回退到嘉立创 EDA 派生副本直接修改，而不是在损坏的 KiCad 工程上继续

### Requirement: 参考设计最小修改原则
派生设计 SHALL 默认继承参考工程在其样机中已使用且不受 Cadenza 差异影响的电源、主控、音频、存储、板框、孔位和布局；任何修改 SHALL 记录原状态、修改内容、依据和风险。

#### Scenario: 审查未受影响子系统
- **WHEN** 某参考子系统与屏幕、输入、USB或结构变更没有真实冲突
- **THEN** 该子系统保持原连接和相对布局，不因通用风格偏好重新设计

### Requirement: L1 Memory LCD 支持
L1 SHALL 直接支持 Sharp LS027B7DH01 的 10-pin、0.5 mm FPC、4.8–5.5 V VDDA/VDD、约 3 V 数字输入、外部 COM 反转和显示开关，并 SHALL 删除原 ST7789 背光与不再需要的显示专用电路。

#### Scenario: 核对十个 FPC 引脚
- **WHEN** L1 原理图进入 PCB 布局前审查
- **THEN** 引脚顺序 SHALL 为 `SCLK, SI, SCS, EXTCOMIN, DISP, VDDA, VDD, EXTMODE, VSS, VSSA`，且连接器接触面和 Pin 1 已由屏幕实物确认

#### Scenario: 显示供电和去耦
- **WHEN** LS027B7DH01 上电
- **THEN** VDDA/VDD 保持在 4.8–5.5 V，VDD 与 VDDA 同时上升或 VDD 先上升，并在连接器附近具有数据手册要求的去耦和测试点

#### Scenario: COM 反转持续运行
- **WHEN** 屏幕处于正常显示或静态保持状态
- **THEN** EXTCOMIN SHALL 在数据手册允许范围内持续翻转，并在任务异常或恢复后具有可测试行为

### Requirement: L2 Cadenza 输入
L2 SHALL 在左侧提供独立 `B` 与 `Menu` 按键，并在右侧提供真实两相旋转编码器和中心按压 `A`；应用层 SHALL 不依赖物理 GPIO 编号理解这些动作。

#### Scenario: 左侧输入
- **WHEN** 用户分别按下 `B` 或 `Menu`
- **THEN** 板级输入层分别产生 Back 或 Menu 动作，且两个按键不共享同一物理输入

#### Scenario: 右侧输入
- **WHEN** 用户顺时针/逆时针旋转编码器或轴向按压旋钮
- **THEN** 板级输入层分别产生正/反 Navigate 和 Confirm/Primary 动作

### Requirement: 独立电源与锁屏按键
L1 SHALL 提供一个独立瞬时 `Power/Lock` 按键；主电源关闭时该按键 SHALL 由常供电的电源控制路径响应，主控运行时同一物理按键 SHALL 产生可供固件处理的独立输入事件，并 SHALL 通过隔离避免电源控制引脚向掉电 ESP32 反向供电。

#### Scenario: 关机状态短按
- **WHEN** 设备由电池供电且主 5 V/3.3 V 电源已关闭，用户短按 `Power/Lock`
- **THEN** IP5306 或等效常供电控制器开启主电源，ESP32 正常启动，且开机能力不依赖已断电的固件

#### Scenario: 运行状态短按
- **WHEN** ESP32 正常运行且用户短按 `Power/Lock`
- **THEN** 板级输入层产生一次去抖后的 Power/Lock 事件，固件可在锁屏与解锁状态之间切换，且短按不会意外切断主电源

#### Scenario: 运行状态长按
- **WHEN** 用户持续按住 `Power/Lock` 超过硬件长按门限
- **THEN** 固件优先尝试保存状态并有序关机；即使固件无响应，电源控制器仍可关闭主电源

#### Scenario: 电源键电气隔离
- **WHEN** 主电源关闭、USB 插入或按键被持续按住
- **THEN** ESP32 GPIO、IP5306 KEY 和电源轨均不超过数据手册限制，不通过 GPIO 保护二极管产生不可接受的反向供电

### Requirement: USB 能力如实声明
派生设计 SHALL 在布板前选择 CH340C USB-UART 或 ESP32-S3 原生 USB 路线，两个 PHY SHALL NOT 直接并联；制造文件和验收报告 SHALL 如实声明是否支持原生 USB/UAC2。

#### Scenario: 保留 CH340C
- **WHEN** USB-C 数据连接到 CH340C
- **THEN** 设计 SHALL 验证供电、串口下载和自动复位，并明确标记不提供 ESP32-S3 原生 USB/UAC2

#### Scenario: 使用原生 USB
- **WHEN** USB-C D-/D+ 连接到 ESP32-S3 GPIO19/GPIO20
- **THEN** 设计 SHALL 验证下载模式、CDC/JTAG和目标 USB 设备固件，且 GPIO19/GPIO20 不再分配给按键或其他外设

### Requirement: 可选麦克风不阻断核心板
麦克风 SHALL NOT 成为 L1/L2 的生产阻断项；若装配麦克风，则料号、封装、引脚和固件接口 MUST 一致，否则 SHALL 标记 DNP。

#### Scenario: 麦克风资料不一致
- **WHEN** 原理图料号、页面说明和实物料号不能对应
- **THEN** 麦克风位 SHALL 标记为待验证或 DNP，且屏幕、输入、USB和扬声器仍可独立验收

### Requirement: 嘉立创生产交付
生产候选版本 SHALL 输出并交叉核对 Gerber、钻孔、BOM、CPL、装配图和制造说明，且 SHALL 在嘉立创 EDA 专业版中完成回导与生产预览检查。

#### Scenario: 生成首板下单包
- **WHEN** PCB 准备提交嘉立创制作或贴装
- **THEN** Gerber 层和钻孔完整、BOM 与 CPL 设计ator集合一致、极性与旋转已检查、所有待手工装配和 DNP 项已列明

### Requirement: 风险分级不得否定样机证据
审查 SHALL 将发现分为适配阻断、首板观察和可选优化；只有得到原文件、数据手册或实测支持的真实失效路径才 SHALL 阻止打板。

#### Scenario: 自动工具产生告警
- **WHEN** ERC、DRC、SPICE、EMC或 DFM 工具报告问题
- **THEN** 该问题先与原文件、器件数据手册和参考样机用途交叉核对，再决定严重级别
