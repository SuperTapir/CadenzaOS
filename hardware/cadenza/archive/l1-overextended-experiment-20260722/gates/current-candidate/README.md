# Current Candidate Gate

这个入口重复运行当前 Cadenza L1/L2 候选证据，不修改参考工程、正式 KiCad
文件或 OpenSpec 任务。

运行：

```bash
python3 hardware/cadenza/gates/current-candidate/run_current_candidate_gates.py
```

脚本会自动探测可用 Python、KiCad、OCP、bundled PDF Python、ngspice、OpenSpec
和 C 编译器；OCP 验证器优先使用调用方已验证的 `PYTHONPATH`，避免跨 Python ABI。

持久输出：

- `latest.json`：机器可读的命令、退出码、运行时、证据、冻结状态和待验证项。
- `latest.md`：给人阅读的摘要。

## 包含的门禁

- L1 显示 10-pin 映射、电压域、电容和 GPIO 候选验证。
- 三种 FPC 座候选封装的焊盘、Pin 1、尺寸和 3D 模型路径验证。
- L1 外壳 fit-check 审计与四种 FPC 出线/接触面候选验证。
- L2 输入 RC 的 ngspice 三个容差角落验证。
- L2 B/Menu/编码器+A 三种机械候选验证。
- L1 原理图差异合同相对冻结参考基线的 `47/47` 一致性验证。
- L1 schematic dry-run 的编辑前基线证明：核对 `SOURCE_MANIFEST.sha256` 的 39 个
  文件、参考连通语义、既有 ERC 分布和预编辑 PDF。PASS 只说明编辑起点可重复，
  **不表示 L1 差异已经实施**。
- L1 candidate-netlist 验证器的 11 个 synthetic fixtures 自测。它只验证门禁能接受
  合格/等价值 fixture 并拒绝指定错误，固定记录 `real_candidate_validated=false`；
  synthetic PASS **不是**真实 L1 候选已经实施或通过的证据。
- 正式两页 `derived/l1-candidate/Cadenza-L1.kicad_sch` 的真实 KiCad XML 网表
  `20/20` 合同验证；它不包含或证明已同步 PCB。
- PCB 同步前置门禁：参考板 77 个 footprint 与候选 94 个实体器件交叉检查；新增
  26 个器件已有 24 个 footprint，只剩 `J_DISP1` 与 `SW_PWR1` 等实物冻结。
- `R_PWR6` 4 × 2 mm net-tie 和 13 个 1.5 mm 裸铜测试点几何/BOM/CPL 排除属性验证。
- 正式候选相对 691 项导入基线的 ERC error UUID 差分；要求新增 error 必须为 0，
  不要求把参考导入债务伪装成“ERC 0”。
- L1 打印/装配风险登记重新生成，要求 9 项风险继续保持待真实验证和未冻结状态。
- L2 可打印 B/Menu 键帽、A 旋钮与 `3×3` 组合候选的 STEP/STL 验证。
- A4 1:1 FPC 座/卡尺记录 fit sheets 的 SVG、PDF、来源哈希和
  `orientation_frozen=false` 验证；使用 Codex bundled PDF Python。
- L2 输入逻辑 host C 测试：`cc -std=c11 -Wall -Wextra -Werror`，编译和执行产物
  都留在 `mktemp` 目录。
- Power/Lock 两种机械候选验证。
- `GT-TC020A-H035-L1` 数据手册专用 S1 footprint：焊盘、孔位、KiCad 10 解析、
  `X/Y/Z` 轴向语义及 `H035` 属于 Y 方向的断言。
- S1 top-edge/right-edge 两方向真实尺寸 fit-check：0.20±0.10 mm 行程、10 个 STEP、
  旋转后轴向和硬碰撞验证；继续保持未冻结。
- 参考原理图网表与 PCB 的 58/58 端点拓扑一致性验证。
- `adapt-oshwhub-handheld-hardware` OpenSpec strict 验证。

## 非破坏性边界

脚本自动探测可导入 `OCP` 的 CAD Python/PYTHONPATH，以及可导入 `pcbnew`
的 KiCad Python/PYTHONPATH。会写局部结果的旧验证器全部在 `mktemp` 临时镜像
中运行，结束后删除镜像。

以下输入在运行前后还会做 SHA-256 清单比较：

- `hardware/cadenza/golden-import/`
- `hardware/cadenza/working-base/`
- 参考项目 `顶盖V7.step` 与 `底盒V7.step`
- `openspec/changes/adapt-oshwhub-handheld-hardware/`
- L1 原理图差异合同与验证器
- L1 schematic dry-run 目录及 `SOURCE_MANIFEST.sha256` 列出的 39 个来源文件
- 正式 L1 两页原理图候选与其证据目录
- R_PWR6 高电流桥接候选资料
- L1 candidate-netlist validator、self-test 和 11 个 fixture
- L1 ERC error 差分验证器
- S1 Power/Lock 厂家 PDF、候选 footprint 与真实尺寸 fit-check
- L2 可打印控制件候选目录
- physical-evidence fit sheets 与其组合 PDF
- L2 input firmware 源码及 host tests

## 不纳入日常门禁的构建

L2 input firmware 的 ESP-IDF 5.5/ESP32-S3 完整构建已经独立通过，证据记录在其
`README.md`，并转录到 `latest.json` 的
`evidence.l2_input_firmware.esp_idf_full_build`。统一候选门禁不会重复执行该完整构建，
也不会执行 `flash`、`esptool` 或任何串口写入。

门禁 `PASS_CANDIDATE` 只表示上述候选检查在当前环境可重复通过。它明确不表示：

- FPC 方向或具体连接器已经冻结；
- L1/L2 外壳已通过实物装配；
- 正式派生 PCB 已完成；
- 可以直接向嘉立创下单或进入量产。
