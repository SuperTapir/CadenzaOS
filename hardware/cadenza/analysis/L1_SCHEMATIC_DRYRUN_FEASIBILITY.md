# L1 KiCad 原理图 dry-run 可行性调查

日期：2026-07-22  
状态：**可实施，但推荐“隔离副本 + KiCad Eeschema 编辑 + 网表差分门禁”，不推荐当前直接由脚本从头生成候选原理图。**

本调查只检查方法和工具，没有创建 L1 dry-run 工程，没有修改 `working-base`、`golden-import` 或 OpenSpec。

## 1. 结论

可以从 `working-base` 安全建立 L1 原理图 dry-run，并同时满足：

- `J_DISP1` 使用 1×10 电气符号，但 `Footprint` 属性保持空白；
- 不选择 FPC 上接、下接、触点面或 Pin 1 物理方向；
- 按 `l1-schematic-delta-contract.json` 实施 Sharp 显示和 Power/Lock 电气网络；
- 保持 `working-base` 与 `golden-import` 字节不变；
- 用 KiCad 10 自己重新解析、导出网表和运行 ERC，而不是只相信第三方解析器。

推荐的编辑主路径是 KiCad 10 原理图编辑器。脚本负责复制前后哈希、候选网表合同、ERC 差分和 PDF 可视化门禁。原因是当前命令行工具具备可靠的读取、ERC 和导出能力，但没有原理图编辑命令；现有第三方库尚不足以证明对这份 KiCad 10 / EasyEDA 导入文件进行复杂新增器件时可完整维护嵌入符号、UUID、实例和连接语义。

## 2. 已验证的本机能力

### KiCad

- 安装版本：KiCad CLI `10.0.4`。
- working-base 原理图格式：`20260306`，generator `eeschema 10.0`。
- `kicad-cli sch` 支持：
  - `erc`，可输出 JSON，并可用 `--exit-code-violations`；
  - `export netlist`，可输出 `kicadxml`；
  - PDF、SVG、BOM 等导出；
  - `upgrade`；
  - **不包含创建、放置、删除或连线等原理图编辑命令。**

只读试跑结果：

| 检查 | 结果 |
|---|---:|
| XML 网表导出 | 成功 |
| PDF 导出 | 成功；有 Fontconfig 警告，但文件生成成功 |
| 原理图真实器件 | 77 |
| 网表网络 | 102 |
| ERC JSON 输出 | 成功 |
| 输入原理图导出前后 SHA-256 | 完全相同 |

### 当前基线 ERC 的含义

working-base 现有 ERC 不是零：共 691 项，其中 18 个 error、673 个 warning。

| ERC 类型 | 数量 |
|---|---:|
| endpoint_off_grid | 389 |
| pin_to_pin | 187 |
| unconnected_wire_endpoint | 61 |
| lib_symbol_mismatch | 33 |
| pin_not_driven | 14 |
| power_pin_not_driven | 4 |
| footprint_link_issues | 3 |

这些主要来自 EasyEDA 导入语义。因此 dry-run 不能以“ERC 必须为 0”作为现实门槛。正确门槛是：

1. 保存完整 baseline ERC JSON；
2. 对候选结果做稳定的差分；
3. 不允许未解释的新 error；
4. 对合同修改区域出现的新 warning 逐项人工审查；
5. 原有违规可以减少，但不能用总数减少掩盖新增的严重问题。

### Python / API 调查

| 工具 | 调查结果 | 结论 |
|---|---|---|
| KiCad `pcbnew` Python | 本机只提供 PCB SWIG API | 不能作为原理图编辑方案 |
| KiCad IPC API / `kipy` | KiCad 可启用 IPC API，但本机未发现可导入的 `kipy` 原理图客户端 | 当前不能依赖 |
| `kiutils 1.4.8` | 已安装，元数据只笼统声明 KiCad 6+；解析模型会忽略未识别 token | 不用于写回本 KiCad 10 文件 |
| `kicad-skip 0.2.5` | 仓库已有，且曾用于生成 working-base | 可用于有断言的局部操作和读取，但复杂新增前仍需单独证明 |
| KiCad skill S-expression parser | 只读解析可靠 | 用于审计，不作为写回器 |

对 `kicad-skip` 做了 `/tmp` 无修改往返：

- 输入和写回后的 `.kicad_sch` SHA-256 完全一致；
- KiCad 重新导出的 77 个器件、102 个网络及网络端点集合完全一致；
- 在完整复制 `.kicad_pro`、`fp-lib-table`、符号库和 `.pretty` 库后，ERC 分类及数量与 baseline 完全一致。

这个结果证明 `skip` 对“无修改加载/写回”没有造成漂移，也证明建立隔离工程时必须复制完整本地库。它**没有证明** `skip` 能安全生成本次所需的 26 个新器件及其嵌入库符号，所以不能据此跳过 KiCad 重新加载和网表门禁。

## 3. 推荐实施方法

### A. 建立隔离副本

后续真正实施时，新建独立目录，例如：

```text
hardware/cadenza/derived/l1-schematic-dryrun/
```

把 `working-base` 的完整目录内容复制过去，至少包括：

- `.kicad_sch`、`.kicad_pro`、`.kicad_pcb`、`.kicad_prl`；
- `sym-lib-table`、`fp-lib-table`；
- `Cadenza-re-easyedapro.kicad_sym`；
- `Cadenza-re-easyedapro.pretty/`。

第一轮建议保留原文件名，只用目录名区分。原理图内部 `instances` 含有原项目名；过早改项目 stem 会增加实例路径和注释重写风险。若以后必须正式改名，应由 KiCad 的 Save As 流程执行，并重新导出网表验证。

复制前后生成 source manifest，记录 working-base 和 golden 的 SHA-256。所有编辑命令只能接受 derived 路径；若输出路径解析到 `working-base` 或 `golden-import`，脚本应立即退出。

### B. 在 Eeschema 中实施合同

只打开 derived 副本的 `.kicad_sch`：

1. 删除合同指定的 9 个参考器件及其失效连线：`FPC1/U6/C20/C21/Q2/R13/R14/SW1/KEY1`。
2. 对 `SW2` 只断开 pad 5 / GPIO6，并设置明确 no-connect；保留 pad 1/2/3/4/6 的合同状态。
3. 新增 `J_DISP1`，使用普通 1×10 连接器电气符号；明确编号 1–10。
4. 将 `J_DISP1` 的 `Footprint` 留空，不放入任何 `FH12/FH12A/FH34` 候选封装。
5. 严格按合同连接：
   - 1 SCLK → GPIO48
   - 2 SI → GPIO12
   - 3 SCS → GPIO14
   - 4 EXTCOMIN → GPIO47
   - 5 DISP → GPIO39
   - 6 VDDA → +5V
   - 7 VDD → +5V
   - 8 EXTMODE → +5V
   - 9 VSS → GND
   - 10 VSSA → GND
6. 新增三颗显示电容、七个显示测试点和完整 Power/Lock 候选网络。
7. 显式填写合同 designator。不要对整张参考原理图执行全局重新注释。
8. 保存并关闭 Eeschema，再由 CLI 重新载入导出。

这个 dry-run 只冻结 Pin number → net 的电气关系，不冻结焊盘朝向。`J_DISP1 Footprint = 空` 是刻意设置的门禁，不是遗漏。

### C. 机器门禁

后续需要新增一个候选网表验证器；现有 `verify_l1_schematic_delta_contract.py` 只证明合同内部一致，不能证明候选原理图实现了合同。

候选验证器至少检查：

- baseline 原理图和网表哈希仍匹配合同；
- 候选真实器件数为 `77 - 9 + 26 = 94`；
- 删除、保留、局部修改、新增 designator 集合完全符合合同；
- `J_DISP1` footprint 缺失或为空；
- `J_DISP1` 1–10 每个 pin 的网络逐项精确匹配；
- 三颗显示电容的端点和值满足合同；
- Power/Lock 二极管、MOSFET、按键、电阻和测试点端点满足合同；
- 67 个 retain 器件的 value、footprint 和非允许网络端点不变；
- SW2 只有 pad 5 是允许变化；
- CH340C、MAX98357A、microSD、ESP32 最小系统等受保护子系统端点不变；
- 不出现 `3V3M → Sharp pin 6/7/8` 或 `+5V → GPIO`；
- 候选 netlist 可被 KiCad 10 成功导出。

## 4. 最小验证步骤

1. 合同自检通过，并记录 baseline 三个项目文件及网表 SHA-256。
2. 复制完整 working-base 到新的 derived 目录；确认源和副本初始哈希相同。
3. 只在 derived 中编辑；保存后再次确认 working-base/golden 哈希未变。
4. 使用 KiCad CLI 导出候选 XML 网表。
5. 运行候选网表合同验证器，要求所有结构性检查通过。
6. 使用 KiCad CLI 输出候选 ERC JSON，与 691 项 baseline 做差分；禁止未解释的新 error。
7. 导出 PDF 或 SVG，人工检查没有重叠、孤立线头、错误 no-connect 和难以阅读的网络。
8. 用 KiCad 10 GUI 重新打开、关闭并再次导出网表；两次候选网表端点集合必须一致。
9. 只有上述步骤完成后，才允许把原理图变化同步到一个**同样隔离的** PCB 副本。
10. 此时仍不选择 FPC footprint，也不勾选需要实物方向证据的 OpenSpec 任务。

## 5. 不可逆或高代价风险

| 风险 | 后果 | 控制措施 |
|---|---|---|
| 误开 working-base 并保存/升级 | 冻结参考被改写 | 编辑器只打开 derived；前后哈希门禁 |
| 只复制三大项目文件，不复制本地库 | ERC 从 691 漂移到 765，出现额外 footprint/library 问题 | 必须复制完整目录依赖 |
| 用文本替换或不完整 S-expression 写回器 | 丢失 KiCad 10 token、UUID、嵌入符号或实例 | 用 Eeschema 编辑；KiCad 自己重新载入验证 |
| 过早重命名项目 stem | `instances/project/path` 改写或注释漂移 | 第一轮保留 stem；改名另设门禁 |
| 全局重新注释 | 参考 designator 大范围变化，PCB 关联失效 | 只显式填写新增 designator |
| 在 dry-run 阶段 Update PCB | 可能删除/改写既有 footprint 与铜线 | 原理图 dry-run 未过门禁前禁止同步 PCB |
| 给 J_DISP1 选择候选 FPC footprint | 把未知触点方向冻结成事实 | 强制 footprint 为空并机器检查 |
| 只看 ERC 总数 | 新错误可能被旧警告减少所掩盖 | 按 UUID、类型、严重度和位置做差分 |

## 6. 本轮没有完成的事情

- 没有创建或修改 L1 dry-run KiCad 工程。
- 没有实施 9 个删除、SW2 局部修改或 26 个新增器件。
- 没有选择 FPC footprint。
- 没有同步 PCB。
- 没有修改 OpenSpec 或勾选任务。
- 没有证明任何候选设计可直接投产。

下一步若获准实施，应先建立 candidate-netlist validator，再建立 derived 副本并通过 Eeschema 完成原理图 dry-run。这样验证器不是在设计完成后临时迎合结果，而是先定义可审计的成功条件。
