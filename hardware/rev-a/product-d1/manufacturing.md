# Cadenza Rev A D1 制造、导入与总装 Draft

状态：v40 PCB 已完成生产 DRC 和制造文件导出；JLCEDA v40 对照、完整 BOM、
屏幕连接器实物验配和机械样品仍未通过，因此仍不能下单。

## 1. 权威源与版本边界

- 主板原理图权威源：`kicad/cadenza-d1.kicad_sch`。
- 当前 PCB 发布候选：`kicad/production-v40-final/cadenza-d1-v40.kicad_pcb`；
  在 JLCEDA v40 对照通过前不覆盖无版本文件。
- 右侧滚轮+A 子板：`kicad/encoder-a/cadenza-encoder-a.kicad_sch` 与 `.kicad_pcb`。
- 外壳参数源：`mechanical/cadenza-d1-enclosure.scad`。
- 历史 `.dsn/.ses` 仅为布线交换记录，不是发布源；Freerouting 2.2.4 在此板
  上出现内部异常，最终关键网与覆铜由 KiCad/Python 可审计流程完成。
- EasyEDA Pro/嘉立创工程是制造复核副本，不反向覆盖 KiCad 权威源。

## 2. v40 主板基线

1. 四层顺序为 `F.Cu / GND(In1.Cu) / POWER(In2.Cu) / B.Cu`。
2. ESP32-S3 模组右上天线区 `x=141.75..166.00, y=49.75..67.25`
   在四层均禁止走线、过孔和覆铜；外壳同一区域禁止电池、扬声器磁钢、
   金属紧固件、热熔铜螺母和导电涂层。
3. LCD 与 PDM 关键网已经绕开 RF 区；地平面子区通过经 DRC 验证的 GND
   stitching 连接。
4. 四个 NPTH 安装孔均在板内；v38 的 `H2` 在板外，已在 v40 移至
   KiCad 坐标 `(93.5,57.5)`，v38 禁止制造。
5. `drc-production-v40-candidate.rpt` 结果为 `0 DRC violation`、
   `0 unconnected`、`0 footprint error`。
6. 发布前仍需人工检查 J2 pin 1、USB-C 舌片方向、microSD 插卡方向、
   电池极性、麦克风底孔、功放 BTL、天线净空和四孔外壳柱位置。

自动布线只负责得到可编辑起点，不等于完成 PCB。电源转换器的高 di/dt 回路、USB 差分对、天线净空、麦克风声孔和屏幕 FPC 必须由人检查。

## 3. 交付文件

当前生成目录为 `kicad/production-v40-final/`。Gerber ZIP、PTH/NPTH、
CPL、IPC-D-356、板统计、DRC 和 PCB STEP 均来自同一个 v40 PCB。
STEP 已按冻结料号补齐 `SW1/J1/J4/J5/U2/U5` 的本地 3D 模型；板框、孔位和已加载模型
可用，但在补齐这些模型前不得用它证明全部器件高度净空。

每块板都建立独立发布目录，至少包含：

| 文件 | 用途 |
| --- | --- |
| Gerber X2/RS-274X ZIP | 铜层、阻焊、丝印、板框制造 |
| Excellon drill ZIP | PTH/NPTH 钻孔和槽 |
| BOM CSV | `Designator, Quantity, Manufacturer Part, LCSC Part` |
| CPL/Position CSV | `Designator, Mid X, Mid Y, Layer, Rotation` |
| 原理图 PDF | 装配与维修核对 |
| IPC-356（若导出） | 独立网表复核 |
| STEP | PCB/器件与外壳碰撞检查 |
| DRC/ERC 报告 | 发布证据 |

Gerber 采用板厂要求的坐标和零点；BOM 与 CPL 必须来自同一次 PCB 提交。面板、扬声器、电池、EC12 和所有线束属于手工/二次装配，不应误放进 SMT CPL。

## 4. 导入 EasyEDA Pro / 嘉立创

推荐保留两条路径并交叉检查：

### 路径 A：导入 KiCad 工程

1. 打开 [EasyEDA Pro 编辑器](https://pro.lceda.cn/editor)，创建新的 D1 工程，不覆盖 D0。
2. 使用文件导入功能选择 KiCad PCB；若编辑器要求工程包，同时提供 `.kicad_pcb`、`.kicad_sch` 和项目表文件。
3. 导入后不要立即下单，逐项比对：板框尺寸、四层顺序、过孔/槽、网络名、封装朝向、丝印、keepout 和 3D 高度。
4. 对私有封装和未在嘉立创库中的器件，保留原焊盘/封装，不允许编辑器按名称自动替换。

### 路径 B：上传制造文件

1. PCB 制造仅上传 Gerber/Drill ZIP。
2. SMT 单独上传 BOM 和 CPL，在选料页按 MPN 与 LCSC 双重核对。
3. 若某个首选器件不可贴装，可冻结焊盘并改为手焊，或选择电气、封装、高度均验证过的替代料；不能只因搜索结果“相似”就替换。

### 导入验收记录

导入后截图并记录以下对象在 KiCad 与 EasyEDA Pro 中的坐标、旋转和层：
`U1/U2/U3/U4/U5/U6/U7/U8/J1/J2/J3/J4/J5/J6/LCD1/H1/H2/H3/H4`。
v40 的 KiCad Edge.Cuts 包络约为 `116.1 x 64.1`；滚轮子板目标为
`34 x 38`。任一尺寸、孔、层或网络不一致都阻断下单。

## 5. 采购分组

- 嘉立创 SMT：ESP32-S3 模组、电源 IC、功放、电量计、ESD、阻容、电感、磁珠、FPC/USB/microSD 连接器及可贴装麦克风。
- 二次焊接：EC12 编码器、线束连接器中不适合 SMT 的型号、必要的 DNP/调试器件。
- 淘宝/其他渠道样品优先：电池、扬声器、Menu/B 开关与键帽、保持型电源开关、硅胶声学垫、泡棉、M2 热熔铜螺母、螺丝和线束。
- 用户自有：`LS027B7DH01` 屏幕；小绿转接板不属于产品 BOM。

采购非嘉立创器件时先买 2-3 个样品，记录实测尺寸、极性、标签和供应链接，再冻结 MPN。除了屏幕，其他器件可以换，但替代必须通过电气、封装、机械和装配验证。

## 6. 总装与首板策略

1. 首次只生产 5 块裸板、2-5 套 SMT；先不批量加工外壳。
2. 不接屏幕和声学件，限流电源检查短路、3.3 V、5 V 使能、充电和关机充电。
3. 烧录测试固件，验证 USB、Wi-Fi/BLE、microSD、滚轮、A/B/Menu、麦克风 PDM 与扬声器 I2S。
4. 用廉价 FPC/转接治具先验证 J2 的接点面和 pin 1，再连接用户屏幕。
5. 装入参数化打印壳，依次完成屏幕、滚轮子板、扬声器声腔、麦克风密封、电池和背壳。
6. 通过电源峰值、音频失真/低频、麦克风灵敏度与啸叫、Wi-Fi RSSI、续航、桌面角度、跌落和 1000 次按压/旋转测试后再冻结 D1。

## 7. 当前阻断项

- 屏幕 J2 实物接点方向尚未完成 1:1 验配。
- 电池、扬声器、Menu/B、电源开关尚未完成实物样品冻结。
- 外壳当前是参数化 Draft，未经过打印装配和声腔测试。
- v38 已导入新的 JLCEDA Project，证明导入流程可用，但 v38 因 `H2`
  板外而禁止制造；必须再导入 v40 并完成四层、板框、四孔、网络、覆铜和
  天线禁铜的截图对照。
- 完整 BOM 仍缺部分 MPN/LCSC 编号；不得仅凭 Value 自动匹配贴片料。
- PCB STEP 已覆盖 6 个关键机械位号；最终外壳碰撞结论仍需实测屏幕、电池、扬声器、键帽和线束。
