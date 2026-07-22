# Cadenza 派生硬件生产就绪状态

> **范围重置提示（2026-07-22）：** 本文以下大部分明细记录的是已经封存的过度扩展候选，其中包含 Power/Lock、USB 和测试点改造，不再代表当前 L1。当前唯一有效状态见 [L1_SCREEN_ONLY_STATUS.md](L1_SCREEN_ONLY_STATUS.md)，旧实验位于 `archive/l1-overextended-experiment-20260722/`。

日期：2026-07-22  
当前结论：**尚不可下单 PCB/PCBA，也尚无最终可打印外壳。** 当前成果是经过检查的
派生候选和实施合同；真实 FPC、EC12、按键和电源行为仍是冻结门禁。

## 继承清单

| 参考设计内容 | 当前处理 | 依据/限制 |
|---|---|---|
| 130 × 60 mm PCB 外轮廓、四个安装孔 | L1 保留 | working-base 与机械审计；正式布局仍需再核对 |
| ESP32-S3-WROOM-1-N16R8 最小系统与天线区域 | 计划原样继承 | 不因 ERC/DRC 数字替代数据手册和最终布局复审 |
| IP5306 电池/升压总体架构 | 条件继承 | KEY 阈值、USB 输入电流与关机语义仍待实测 |
| MAX98357A、扬声器接口 | 计划继承 | 3.3 V I2S 电平已由官方规格核对；最终音腔/EMC未验证 |
| microSD 与参考音频/存储 GPIO | 计划继承 | L1 修改边界禁止无依据改动 |
| USB-C 板边位置、CC 5.1 kΩ、BOOT/RST | 位置/拓扑继承 | 精确连接器 MPN 与外壳焊脚仍待冻结 |
| 参考底盒、固定关系、扬声器和电池包络 | L1 fit-check 继承 | 参考 STEP 无参数历史，必须继续以几何审计保护 |

## 修改清单

| 层级 | 修改 | 当前状态 |
|---|---|---|
| L1 | 删除 ST7789 12P FPC、3V3LCD LDO、背光电路，加入 Sharp 10P、5 V 去耦、EXTMODE、DISP/EXTCOMIN 和测试点 | 已写入正式两页 KiCad 原理图候选，真实网表 20/20；PCB 未同步 |
| L1 | 前壳改为 Sharp 窗口、软垫台阶、后压框和 FPC relief | 四方向 fit-check 已生成，方向未冻结 |
| L1 | 增加独立 Power/Lock 瞬时键与感知/强制关机候选 | 电路与侧按器件已有候选；IP5306 实测未完成 |
| L2 | 左侧 B/Menu，右侧 EC12，中心按压为 A | 三组坐标与电气候选已生成；真实器件未测 |
| L2 | CH340C 转 ESP32-S3 原生 USB，保留手动 UART 救援 | 推荐候选；尚未实施或真机验证 |
| L2 | 参数化键帽与旋钮、前壳承力/开孔 | 21 STEP + 21 STL 候选已生成；不能替代真实轴型和行程测量 |
| L3 | 比例、圆角、握持、格栅、音腔、装配与维护优化 | 必须等待 L1/L2 实物装配通过 |

## 已完成的验证

- 参考 KiCad working-base：77 个器件、58 个有效连接网络，连通集合 `58/58` 匹配，
  0 unconnected；这只证明转换连通性，不证明生产可靠性。
- Sharp 10-pin 电气映射、5 V/3.3 V 电压域、三颗去耦和 GPIO 限制：自动门禁 PASS。
- L1 外壳 fit-check：参考哈希/XY 包络/四孔不变，12 个 STEP 有效；基础与四方向
  候选最大硬碰撞均为 `0 mm³`，语义签名可重复，`selection_frozen=false`。
- 三种 FPC 候选 footprint：焊盘 1–10、pitch、Pad 1、固定焊片与 3D 路径检查 PASS，
  KiCad 10 可解析；FH12A/FH34 的 STEP 仍是明确标注的 `PROXY`。
- FPC/器件实物门禁：已生成四页 A4 1:1 对位与卡尺记录 PDF；源 footprint 哈希、
  12 个焊盘几何、四页 A4 尺寸均验证 PASS，并逐页渲染检查无裁切。打印页只辅助
  断电对位，不能证明 Pin 1 或触点方向，`orientation_frozen=false`。
- L2 输入 RC 候选：ngspice fast/nominal/slow 3/3 PASS；20 µs 合成毛刺被过滤，
  75 µs 以上理想脉冲保留。未包含真实 EC12 波形。
- L2 隔离输入验证固件：host 状态机测试 PASS，ESP-IDF 5.5/ESP32-S3 构建 PASS；
  支持 Gray-code、非法跃迁/队列溢出统计、消抖、短/长按和可配置 `cpd/reverse`。
  本轮未刷写，真实 EC12 脚位、方向和每卡点计数仍待验证。
- L2 可打印控制件：B/Menu 键帽、中心 A 编码器旋钮与 round/D/可替换滚花适配芯，
  三布局 × 三接口共九套装配；21/21 STEP 有效、21/21 STL 封闭，代理按压末端最大
  硬碰撞 `0 mm³`，但 `production_ready=false`。
- L1 原理图修改合同：冻结参考器件 `9 删 + 1 改 + 67 留 = 77`，网络
  `10 删 + 12 改 + 80 留 = 102`，47/47 自检 PASS；它没有修改正式 KiCad。
- L1 原理图 dry-run 可行性：已采用窄范围、可重复的 KiCad 10 派生流程，并以
  网表合同和 ERC 差分门禁复核；导入基线已有 691 项 ERC，不能把“清零”当实施目标。
- L1 原理图隔离副本已建立：39 个 working-base 来源文件初始逐项哈希一致；KiCad
  10.0.4 重导出 77 个器件、102 个网络、691 项 ERC 和一页 A3 PDF 成功。当前仍是
  编辑前旧显示基线，不代表 3.4/3.8 已实施。
- L1 候选网表合同门禁：11/11 synthetic fixture 自测 PASS；正式两页 KiCad 原理图候选
  也由 KiCad 10 导出真实 XML 网表并通过 20/20。94 个实体器件、Sharp 1-10、
  BAT54C/AO3400A pinout、Power/Lock、测试点及受保护端点均符合合同。
  `real_candidate_validated=true` 仅表示原理图结构/网络通过，`pcb_synced=false`。
- 正式候选的 20/20 网表门禁还固定了 H1/Q1 两项参考 PCB footprint 元数据恢复、
  `R_PWR6` net-tie 和 13 个裸铜测试点 footprint；当前只剩 `J_DISP1`、`SW_PWR1`
  两项必须依靠实物才能冻结的 footprint。
- 正式候选 ERC 为 599 项（585 warning、14 error），少于导入基线 691 项（18 error）；
  type + item UUID 差分证明新增 error 为 0、旧显示删除带走 4、其余 14 均继承。多数 warning
  仍是导入 off-grid/pin-to-pin/library mismatch 债务，不能因此称为 ERC 已通过生产审查。
- L1 打印/装配风险登记可重复生成：9 项仍待真实验证（critical 2、high 5、medium 2）；
  这不会冻结 FPC 方向、软垫、压框、螺钉、打印补偿或完整 Z 向堆叠。
- R_PWR6 高电流桥接：首选候选为短而宽的 PCB net-tie 铜桥；备选为 Stackpole
  `RMCF2512ZT0R00` / LCSC `C4103112`（3 A）。备选在 2.1 A、最大 50 mΩ 时约
  105 mV/0.221 W；当前仍未冻结，库存必须在下单日复核。
- S1 Power/Lock 侧按候选已按厂家 `GT-TC020A-H035-L1` 第 1 页建立专用封装：
  2 个电气焊盘、1 个无编号机械焊盘和 4 个定位孔均通过 KiCad 10 检查。主审纠正了
  初版 3D 轴向误读；当前模型明确采用 `X/Y/Z = 4.6/2.3/1.8 mm`，`H035=3.50 mm`
  属于 Y 前后总深度而非 Z 高度。A/B 两方向、0.20±0.10 mm 行程和 10 个 STEP
  重新验证 PASS，仍为 `selection_frozen=false`、`production_ready=false`。
- L1/L2/Power-Lock 机械候选当前硬碰撞检查 PASS；它们是包络检查，不是装配证明。
- KiCad → 嘉立创 EDA 专业版手动回导已保存并只读审计：77 个封装、板框、孔位和铜
  几何仍在，15 处 `Arial` 只触发文字替代告警；但焊盘、走线、过孔和铺铜的网络名
  全部为空，因此该 `.eprj2` 只能作几何预览，不能作为电气主工程或生产 DRC 依据。
- 统一候选门禁 `23/23 PASS_CANDIDATE`，326 个受保护文件运行前后零变化；OpenSpec
  strict validation PASS，当前完成 11/44 项，没有把候选结果误标成冻结完成。

## 风险清单

1. **FPC 方向**：Pin 1、触点面、自然出线与转接板十针连续性未知；错误会导致屏幕
   无法连接，严重时把 5 V 接到信号脚。
2. **EC12 实物**：轴型、轴长、定位脚、引脚、脉冲/卡点和按压行程未知；当前 footprint、
   旋钮与 RC 均不能冻结。
3. **Power/Lock**：IP5306 KEY 的公开资料没有给出完整阈值/内部偏置；BAT54C 压降、
   USB 插入和长按硬关机必须做样机测试。
4. **USB**：L2 原生 USB 尚未实施；连接器精确 MPN、ESD、90 Ω 几何、VBUS 3 ms
   检测和 USB-C 输入电流合规性均待验证。
5. **机械代理**：两个 FPC 连接器 STEP、当前 EC12/开关/旋钮均含代理尺寸；零碰撞不等于
   真件装得进去，也不证明 FDM 打印收缩、软垫压缩和玻璃应力安全。
6. **参考导入 DRC**：EasyEDA 导入板有大量几何/语义型 DRC 告警；已经分诊部分复合焊盘
   误报，但最终派生板仍必须重新完成 DRC、DFM、EMC 与人工复核。
7. **嘉立创可编辑回导**：当前 KiCad 10 → 嘉立创转换保留几何但丢失全部网络归属；
   生产资料必须以 KiCad 导出的 Gerber/钻孔/BOM/CPL 为主，并在生产预览逐项核对。
8. **电池和充电**：最终电芯、连接器、容量、保护与充电约束尚未冻结，外壳电池余量也
   不能作为量产结论。

## 验证清单（下一顺序）

1. 全程断电完成 `validation/physical-evidence/MEASUREMENT_SESSION.md`。
2. 以 `output/pdf/cadenza-physical-fit-sheets-a4.pdf` 按 100% 打印，先量校准尺，再根据
   实测选择 FPC footprint/旋转方向并冻结 L1 机械方向。
3. 在 DevKit 面包板逐线验证 Sharp，记录供电、电流、全屏图、EXTCOMIN 与恢复行为。
4. 对 14 个继承 ERC error 做人工解释/处置记录；禁止借机改动合同外参考电路。
5. 实测并冻结 FPC 后填入 J_DISP1 footprint，再把正式原理图同步到派生 PCB；随后完成
   DRC、数据手册、人工 pinout/封装、电源、PCB/外壳组合与嘉立创回导。
6. 测量 EC12/按键并打印 L2 手感样；再冻结输入坐标、footprint、旋钮和 RC 装配值。
7. 实施 L2 输入及原生 USB，完成真机输入/下载/恢复、USB 插拔和 ESD/EMC 前置验证。
8. 完成 Gerber、钻孔、BOM、CPL、装配图和最终 STEP/STL；在嘉立创生产预览逐项核对。
9. 只下少量工程样机，完成整机验收后再进入 L3 和下一版。

## 缺失资料清单

- 屏幕 FPC、转接板锁扣和十针连续性的清晰照片/测量表。
- ESP32-S3-DevKitC-1 的正反面版本照片，尤其是 GPIO48/RGB LED 状态。
- EC12 正反面照片、全部卡尺尺寸、A/C/B 与中心按键通断、脉冲/卡点实测。
- 6 × 6 × 5 mm 按键三件尺寸样本；Power/Lock 侧按候选的实物样品。
- 最终 USB-C 连接器精确 MPN/机械图，以及选定电池和连接器资料。
- Sharp、Power/Lock、输入、USB 的真机结果；最终打印材料、打印机和孔径补偿。

在上述证据补齐前，任何 BOM、STEP/STL 或检查 PASS 都不能被称作“可直接下单”。
