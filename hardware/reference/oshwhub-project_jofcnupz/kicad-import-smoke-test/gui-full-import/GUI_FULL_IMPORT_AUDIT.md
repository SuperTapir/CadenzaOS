# EasyEDA Pro V2 → KiCad 10.0.4 完整 GUI 导入审计

审计对象：用户在 KiCad 桌面版中导入并保存的冻结 V2 `.epro`。  
审计日期：2026-07-22。  
结论：PCB 几何与网络结构通过导入门禁；完整原理图已导入，但 EasyEDA 的网络别名桥、电源符号和部分封装/铺铜语义需要在独立派生副本中修复后，才能成为可编辑 golden 工程。

## 导入完整性

正式保存目录包含：

- `.kicad_pro`
- `.kicad_sch`
- `.kicad_pcb`
- 项目符号库 `Cadenza-re-easyedapro.kicad_sym`
- 从原 `.epro` 恢复的 25 个已使用封装及 `fp-lib-table`

桌面导入副本仍保留，项目内归档不包含 GUI 锁文件和 `.history`。

## PCB 结构化差异门禁

同一份基线脚本分别检查 CLI 导入 PCB 和 GUI 保存 PCB。GUI 保存版本结果为：

- 77/77 个封装：PASS
- 58/58 个 PCB 网络及完整网络名集合：PASS
- 2/2 个铜层：PASS
- 78/78 个过孔：PASS
- 506/506 段已布铜线：PASS
- 130.000 × 60.000 mm 板框：PASS
- 20 个关键器件、接口和安装孔坐标/角度：全部在 0.001 mm/0.001° 内 PASS
- KiCad DRC 未连接项：0
- 自动检查：28 PASS、0 FAIL

证据：`PCB_DIFFERENCE_AUDIT.md` 和 `pcb-difference-audit.json`。

## 完整原理图导入结果

KiCad BOM 导出证明：

- 原理图中有 77 个真实位号。
- PCB 中有 77 个位号。
- 两侧位号集合完全相等，没有缺失或多余真实器件。
- 另有 21 个被导入为 `?` 的 EasyEDA 绘图/网络辅助对象：1 个 A4 图框对象和 20 个网络别名桥。

20 个网络别名桥的来源语义如下：

| 功能名 | ESP32 侧网络 |
|---|---|
| KEY_MENU | GPIO18 |
| KEY_OPTION | GPIO8 |
| KEY_SELECT | GPIO16 |
| KEY_START | GPIO17 |
| KEY_A | GPIO15 |
| KEY_B | GPIO5 |
| KEY_UP | GPIO7 |
| KEY_LEFT | GPIO19 |
| KEY_DOWN | GPIO20 |
| KEY_RIGHT | GPIO6 |
| SD_CMD | GPIO11 |
| SD_CLK | GPIO13 |
| SD_DATA | GPIO9 |
| SD_CD | GPIO10 |
| TFT_BLK | GPIO39 |
| TFT_MOSI | GPIO12 |
| TFT_CLK | GPIO48 |
| TFT_DC | GPIO47 |
| TFT_RST | GPIO3 |
| TFT_CS | GPIO14 |

EasyEDA 用菱形桥连接两种网络别名；KiCad 将桥当成普通两引脚符号，导致桥两侧在导出网表中被拆开。因而原理图网表暂时有 122 个网络，而 PCB 为 58 个网络，并出现 354 条一致性警告。该问题可由上表确定性修复，不能据此推断参考实物接错。

## ERC 分诊

补齐本地封装库后，导入原理图仍有 828 条 ERC 消息：56 error、772 warning。

| 类型 | 数量 | 解释 |
|---|---:|---|
| endpoint_off_grid | 409 | EasyEDA 坐标单位转换后端点未落在 KiCad 当前连接网格；需在派生副本中重对齐 |
| pin_to_pin | 205 | 导入符号的电气引脚类型大量为 `input/unspecified`；不等于实际电路冲突 |
| footprint_link_issues | 93 | 主要是图框、电源符号等无封装对象被错误写成空封装库路径 |
| pin_not_driven | 52 | 电阻、晶体管等引脚类型被错误导入为输入 |
| lib_symbol_mismatch | 34 | 导入实例与项目符号库副本的转换差异 |
| unconnected_wire_endpoint | 31 | 主要为极短的 EasyEDA 连接辅助线和别名桥端点 |
| power_pin_not_driven | 4 | 导入电源符号没有原生 KiCad power-output/PWR_FLAG 语义 |

这些是“导入原理图尚未原生化”的证据，不是“参考电路有 828 个问题”的证据。

## DRC 分诊

补齐本地封装库后，未修改 PCB 为 192 条 DRC 消息、0 未连接：

- 58 clearance
- 26 solder-mask bridge
- 25 silk-over-copper
- 23 silk overlap
- 17 copper-edge clearance
- 8 silk-edge clearance
- 7 library-footprint mismatch
- 6 shorting-items
- 5 hole-clearance
- 4 annular-width
- 4 padstack
- 2 starved-thermal
- 2 courtyard overlap

主要来源仍是 401 个固定铺铜碎片、U4/U7 复合焊盘、NPTH/机械孔、EasyEDA 丝印和 KiCad 默认规则差异。它们必须在派生副本中逐类处理，但不能倒推为参考样机有同等数量的硬件故障。

## 3D 和模型状态

macOS 26 最初拒绝 KiCad 10.0.4 的 3D 插件和 `_cvpcb.kiface`。该环境问题已独立修复并通过：

- KiCad bundle 深度签名验证。
- ERC 插件加载。
- GUI 启动。
- 顶面、底面和透视 PCB 3D 渲染。

当前 PCB 封装没有绑定完整器件 STEP/WRL，因此渲染可以显示板体、铜、焊盘和工艺层，但不能把“没有器件实体”解释为真实装配高度。

## 继续使用 KiCad 的决定

继续使用 KiCad 作为派生主工程，不回退为只在嘉立创 EDA 中修改，理由是：

1. PCB 的器件、网络、走线、孔、板框和关键坐标全部结构化匹配。
2. 原理图 77 个真实位号完整，转换损失集中在可枚举的别名桥、符号类型和网格语义。
3. KiCad 已能稳定运行 ERC、DRC、BOM、网表和 3D 渲染。
4. 仍保留冻结 EasyEDA 原件，并要求最终回导嘉立创 EDA 做生产终检。

## 形成 golden 工程前必须修复

在新的派生副本中按顺序处理：

1. 删除 A4 图框伪符号。
2. 将 20 个网络别名桥改为原生网络标签/直接连接，并逐对核对上表。
3. 将导入的 GND、3V3M、+5V 等符号替换为原生 KiCad 电源符号，按真实电源输出添加必要 PWR_FLAG。
4. 按器件数据手册修正电阻、电容、晶体管、接口和 IC 的电气引脚类型；不得仅为清零而批量改成 passive。
5. 重建原来的 2 个参数化铺铜，并与冻结渲染逐区比较。
6. 修复 U4 pad 41、U7 pad 3 的复合焊盘网络/阻焊/钢网语义。
7. 人工比较 7 个实例与库封装差异，不能直接“从库更新封装”。
8. 为结构关键件补齐 3D 模型和高度。
9. 修复后重新导出网表，要求 77 个真实位号与 PCB 一致、网络连通集合一致、0 未连接。

当前状态为 `PASS_PCB_IMPORT / FIX_SCHEMATIC_SEMANTICS_BEFORE_EDITING`，尚不是可下单设计。
