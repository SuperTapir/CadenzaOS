## ADDED Requirements

### Requirement: 参考结构基准继承
派生外壳 SHALL 优先继承参考 STEP 中已经与 PCB、螺钉、USB、开关、扬声器和电池配合的总体包络与固定关系，并 SHALL 只修改受屏幕和输入差异影响的区域。

#### Scenario: 建立 L1 外壳副本
- **WHEN** 顶盖和底盒 STEP 被用于 Cadenza
- **THEN** 原 STEP 保持只读，派生模型保留可复用螺柱、底盒和接口基准，并记录被切除或新增的实体

### Requirement: L1 屏幕安全安装
L1 外壳 SHALL 适配 62.8×42.82×1.64 mm 的 LS027B7DH01 和 58.8×35.28 mm 可视区，并 SHALL 通过周边定位、软垫和受控压持避免玻璃或 FPC 受力。

#### Scenario: 屏幕 fit-check
- **WHEN** 屏幕或等尺寸硬质样片装入打印前壳
- **THEN** 可视区无遮挡，玻璃不翘曲，FPC 不接触玻璃锐边，并满足数据手册规定的弯折区和最小半径

### Requirement: L2 输入结构
L2 前壳 SHALL 在左侧提供独立 `B` 和 `Menu` 键帽，在右侧提供编码器旋钮和中心 `A` 的操作区域；编码器 SHALL 由 PCB 固定脚和壳体轴向/径向结构共同承力。

#### Scenario: 按键与滚轮操作
- **WHEN** 用户正常握持并操作 B、Menu、旋转和中心按压
- **THEN** 所有动作可达、互不遮挡屏幕、没有持续摩擦或明显误触，PCB 焊盘不承担主要机械载荷

### Requirement: 可打印独立零件
旋钮、B 键帽、Menu 键帽、Power/Lock 按帽及必要的屏幕压框 SHALL 以独立 STEP/STL 输出，并 SHALL 记录适用的开关/编码器轴型、打印方向、间隙和后处理要求。

#### Scenario: 旋钮打印装配
- **WHEN** 按推荐材料和方向打印旋钮并安装到目标 EC12
- **THEN** 旋钮不空转、不永久卡死、可完成全行程按压，并与前壳保持可重复装配间隙

### Requirement: PCB 与外壳共享机械基准
PCB 板框、安装孔、屏幕窗口、FPC、编码器轴、正面按键、Power/Lock 按键、USB、维护开关和扬声器开孔 SHALL 来源于同一组版本化机械参数或可自动比较的坐标表。

#### Scenario: 生成装配模型
- **WHEN** 同一版本的 PCB STEP 与外壳 STEP 组合
- **THEN** 安装孔、屏幕、输入轴和接口开孔在记录公差内对齐，且没有硬碰撞

### Requirement: 电源键结构与防误触
外壳 SHALL 为 `Power/Lock` 提供可单手触及、不会与 B/Menu/A 混淆的侧边或顶边位置，并 SHALL 通过导向、限位和适度凹陷降低携带和握持时误触风险。

#### Scenario: 正常短按与长按
- **WHEN** 用户握持设备并短按或持续按住 Power/Lock
- **THEN** 按帽沿预定方向运动、能够释放回弹、不会顶住 PCB 或壳体，并可可靠达到目标开关行程

#### Scenario: 携带防误触
- **WHEN** 设备平放、放入口袋或正常握紧外壳边缘
- **THEN** 外壳表面不会轻易持续压住 Power/Lock，且用户仍可有意识地找到并操作该键

### Requirement: L3 工业设计优化受功能门禁约束
L3 SHALL 在 L1/L2 的屏幕安装、输入手感、接口插拔和外壳闭合通过后，才能优化比例、圆角、握持、表面、格栅、螺钉和装配细节。

#### Scenario: 提交 L3 外观版本
- **WHEN** L3 STEP/STL 被标记为生产候选
- **THEN** 它继承已通过的 L1/L2 机械接口，并附有打印验证、装配照片和剩余待验证项

### Requirement: 可维护和非破坏拆装
外壳 SHALL 允许在不破坏屏幕、FPC、PCB 或电池的情况下打开并更换主要部件。

#### Scenario: 拆装维护
- **WHEN** 按装配说明打开工程样机
- **THEN** 可依次断开电池、扬声器和屏幕 FPC并取出 PCB，过程不需要撬碎胶粘件或拉扯 FPC
