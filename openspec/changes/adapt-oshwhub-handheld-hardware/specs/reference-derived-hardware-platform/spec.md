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

#### Scenario: L1 PCB 差异边界
- **WHEN** L1 PCB 与冻结参考基线比较
- **THEN** `USB1`、`U4`、板框和四个安装孔 SHALL 保持位置与物理几何不变，除 `C20/C21` 显示去耦和被替换显示器件外的冻结器件 SHALL 保持物理几何；USB `D+`、`D-`、`CC1` MAY 局部绕行，但连接器焊盘网络和端点 SHALL 不变

### Requirement: L1 Memory LCD 支持
L1 SHALL 直接支持 Sharp LS027B7DH01 的 10-pin、0.5 mm FPC、4.8–5.5 V VDDA/VDD、约 3 V 数字输入、外部 COM 反转和显示开关，并 SHALL 删除原 ST7789 背光与不再需要的显示专用电路。

#### Scenario: 核对十个 FPC 引脚
- **WHEN** L1 原理图进入 PCB 布局前审查
- **THEN** 引脚顺序 SHALL 为 `SCLK, SI, SCS, EXTCOMIN, DISP, VDDA, VDD, EXTMODE, VSS, VSSA`，且连接器接触面和 Pin 1 已由屏幕实物确认

#### Scenario: L1 显示 GPIO 与连接器坐标
- **WHEN** 生成 L1 PCB 候选
- **THEN** `J_DISP1` SHALL 位于 `(154.625 mm, 128.500 mm)` 且旋转 `0°`，并 SHALL 使用 `SCLK=GPIO39`、`SI=GPIO48`、`SCS=GPIO47`、`EXTCOMIN=GPIO14`、`DISP=GPIO12`；屏幕和窗口 SHALL 相对严格居中位置采用 PCB `X +1.5 mm`、KiCad `Y -1.5 mm` 偏移

#### Scenario: 显示供电和去耦
- **WHEN** LS027B7DH01 上电
- **THEN** VDDA/VDD 保持在 4.8–5.5 V，VDD 与 VDDA 同时上升或 VDD 先上升，并在连接器附近具有数据手册要求的去耦；测量可通过连接器焊盘或现有可达节点完成，不强制增加独立测试点

#### Scenario: COM 反转持续运行
- **WHEN** 屏幕处于正常显示或静态保持状态
- **THEN** EXTCOMIN SHALL 在数据手册允许范围内持续翻转，并在任务异常或恢复后具有可测试行为

### Requirement: 导入分裂焊盘语义可追踪
EasyEDA Pro 导入造成的同号分裂焊盘 SHALL 在不移动器件和不改变焊盘物理几何的前提下恢复为预期网络语义，并 SHALL 在嘉立创回导后再次检查。

#### Scenario: 归一 U4 与 U7 重复焊盘
- **WHEN** L1 候选重新填充铜皮并计算连通性
- **THEN** `U4.41` 与 `U7.3` 的全部同编号物理片段 SHALL 分别统一到 `GND` 并具有有效铺铜连接，归一前后 `U4` 和 `U7` 的器件位置与焊盘几何 SHALL 不变，候选 SHALL 不再包含同一物理引脚内部的 `shorting_items`

### Requirement: PCB 候选证据不得越界
L1 PCB 回归检查 SHALL 同时保存参考与候选原始报告，并 SHALL 将“静态候选通过”与“可下单”分开陈述。

#### Scenario: L1 PCB 回归门禁通过
- **WHEN** L1 候选报告边界检查 `25/25 PASS`、`0 unconnected`、`0 shorting_items`，且 DRC 从参考导入基线 `192` 项变为候选 `95` 项并在 Sharp 编辑区域无新增问题
- **THEN** 该版本 MAY 标记为 `PASS_CANDIDATE`，但 SHALL NOT 标记为 production ready，直至真实 FPC 方向/折弯/锁扣/合壳和嘉立创制造回导全部完成

### Requirement: L2 Cadenza 输入
L2 SHALL 在左侧提供独立 `B` 与 `Menu` 按键，并在右侧提供真实两相旋转编码器和中心按压 `A`；应用层 SHALL 不依赖物理 GPIO 编号理解这些动作。

#### Scenario: 左侧输入
- **WHEN** 用户分别按下 `B` 或 `Menu`
- **THEN** 板级输入层分别产生 Back 或 Menu 动作，且两个按键不共享同一物理输入

#### Scenario: 右侧输入
- **WHEN** 用户顺时针/逆时针旋转编码器或轴向按压旋钮
- **THEN** 板级输入层分别产生正/反 Navigate 和 Confirm/Primary 动作

### Requirement: L1 继承参考电源操作
L1 SHALL 保持参考设计的电源控制器、开关、相关 GPIO 和外壳开口，不得为新增 `Power/Lock` 扩大首板修改范围。

#### Scenario: L1 电源控制审查
- **WHEN** L1 与参考原理图、PCB 和外壳进行差异比较
- **THEN** 除显示负载造成的必要电源连接外，电源控制网络、开关位置和机械操作方式保持不变

### Requirement: L2 可选 Power/Lock
L2 MAY 增加独立瞬时 `Power/Lock`；只有在常供电唤醒、运行时 GPIO 事件、掉电隔离和外壳防误触均有简单且可验证的实现时才 SHALL 纳入，否则 SHALL 继续使用 L1 已验证的参考电源方式。

#### Scenario: 可选方案不满足门槛
- **WHEN** Power/Lock 需要显著重构电源、存在反向供电风险、占用冲突 GPIO 或迫使外壳大改
- **THEN** L2 SHALL 不安装该功能，且不因此阻止 L2 核心输入 MVP 完成

### Requirement: USB 能力如实声明
L1 SHALL 原样继承参考设计的 CH340C USB-UART 路线、接口位置和下载电路；L2 MAY 单独评估 ESP32-S3 原生 USB。任何版本中的两个 PHY SHALL NOT 直接并联，制造文件和验收报告 SHALL 如实声明是否支持原生 USB/UAC2。

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
