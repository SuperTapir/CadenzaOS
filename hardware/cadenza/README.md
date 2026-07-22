# Cadenza 硬件派生工程

本目录保存从已打样运行的 OSHWHub ESP32-S3 掌机参考工程派生的 Cadenza 硬件与外壳。冻结下载证据仍在 `hardware/reference/oshwhub-project_jofcnupz/`，不会原位修改。

当前整体结论、继承/修改/风险/验证/缺失资料清单见 `PRODUCTION_READINESS.md`。

## 目录状态

- `golden-import/`：用户在 KiCad 10.0.4 GUI 中完成的完整 EasyEDA Pro V2 导入副本。该目录保持逐字节冻结，不做任何修改。
- `working-base/`：从 golden 副本生成的可工作基线；只修复 EasyEDA 辅助符号、网络别名和电源符号的 KiCad 语义，不改变参考 PCB 或 77 个真实器件。
- `derived/l1-screen-only/`：当前唯一 L1 工作副本；从 working-base 完整复制，初始
  39 个来源文件逐项哈希一致，PCB 与 golden 逐字节一致。范围只允许替换 Sharp 屏幕。
- `archive/l1-overextended-experiment-20260722/`：此前混入 USB、Power/Lock、测试点和
  额外布线的旧候选完整归档；不再作为 L1 母版。
- `electrical/`：Sharp、Power/Lock、USB、GPIO 和输入电气候选；未验证内容均保留候选状态。
- `mechanical/shared/`：Sharp 共享机械基准；`mechanical/l1-fit-check/`、
  `l2-input-candidates/`、`l2-printable-controls/` 与 `power-lock-candidates/` 保存非冻结
  结构方案和可切片试印件。
- `validation/`：断电测量、屏幕面包板步骤、结果模板和隔离的 ESP-IDF 测试固件。
- `gates/`：L1 修改边界与 EasyEDA 导入复合焊盘语义规范化工具。
- `manufacturing/`：嘉立创制造 profile 和相对参考板的 BOM 增删候选。
- `libraries/`：经数据手册核对、但尚未由实物方向冻结的本地 KiCad 候选库。
- `sourcing/`：FPC、Power/Lock 开关和新增被动件的厂家资料、LCSC/JLCPCB 快照与
  未冻结候选；库存必须在下单当天重查。
- 正式 `l1/`、`l2/`、`l3/` 生产候选目录只在对应实物门禁通过后建立；现有候选文件
  不因 STEP 可打开或碰撞为零而自动升级为生产版本。

## Golden-import 初始哈希

复制时与 `hardware/reference/.../gui-full-import/` 中用户保存版本逐字节一致：

| 文件 | SHA-256 |
|---|---|
| `.kicad_pro` | `bb2fdd73a376904e22bb7428891c3425cf2eff1e7907036c78450dab097cdb5a` |
| `.kicad_sch` | `58b74354140eec31b419619c28eb47a8e217f2e9ead8e09badaa112ca11785f3` |
| `.kicad_pcb` | `70a46bd100ed8184edcd2e90d3dbd21fff676999e0f3fa71cad1121c8c7b4101` |
| `.kicad_sym` | `74f14190bbd9206441116219ff4546007d31befe4790a2f30aaa2afad0deed13` |

## 修改原则

1. 先恢复 KiCad 原生语义并证明网表连通集合与参考 PCB 一致。
2. PCB 的 77 个封装、58 个网络、78 个过孔、506 段走线、130 × 60 mm 板框和关键坐标在 golden 门禁前保持不变。
3. 任何功能变化进入 L1/L2 目录，并记录继承、修改、依据、风险和验证。
4. ERC/DRC 数字只能作为证据之一，不能单独证明可靠或否定参考实物。
5. 最终必须回导嘉立创 EDA，核对 BOM/CPL、封装、铜皮、3D 和生产预览。

## 当前最近的人工门禁

先执行 `validation/physical-evidence/MEASUREMENT_SESSION.md` 的**全程断电**测量，
确认 FPC 触点/Pin 1/十针连续性，并测量 EC12 与按键。之后才允许冻结 FPC 座、
旋钮、键帽和 Power/Lock 位置，再进行屏幕上电验证与正式 KiCad 功能修改。
辅助打印件位于 `output/pdf/cadenza-physical-fit-sheets-a4.pdf`；必须按 100% 打印并先
核对 10/50 mm 校准尺，打印对位不能替代断电通断测量。

## 当前可重复候选门禁

统一入口：`python3 hardware/cadenza/gates/current-candidate/run_current_candidate_gates.py`。
它在临时镜像中运行会写结果的检查，并对 golden、working-base、参考 STEP 和 OpenSpec
做运行前后哈希保护；`PASS_CANDIDATE` 明确不等于 production ready。

- Sharp 电气映射：`electrical/display/verify_l1_display_baseline.py` → PASS。
- L1 外壳与四种 FPC 方向：`mechanical/l1-fit-check/audit_l1_fitcheck.py` → PASS，
  12 个有效 STEP、最大硬碰撞 `0 mm³`、`selection_frozen=false`。
- FPC KiCad 10 隔离库：`libraries/display-fpc-variants/verify_library.py` → PASS，
  三个候选均可由 KiCad 10 解析；两个模型明确为 `PROXY`。
- FPC 1:1 核对纸：`validation/physical-evidence/fit-sheets/verify_fit_sheets.py` → PASS，
  四页 A4 PDF 已逐页渲染检查，且继续保持 `orientation_frozen=false`。
- L2 输入 RC：`electrical/input/spice/run_spice.py` → 3/3 容差角落 PASS；这只验证
  理想候选电路，不替代真实 EC12 脉宽、相序与抖动测量。
- L2 输入验证固件：`validation/l2-input-firmware/` 的 host tests 与 ESP-IDF 5.5 构建
  PASS，未刷写；真实脚位、方向、每卡点计数仍需实物验证。
- L2 可打印控制件：21 个 STEP + 21 个封闭 STL、三布局 × 三轴接口候选验证 PASS；
  `production_ready=false`，真实轴型和行程测量前只可用于试印。
- S1 Power/Lock：厂家页 1 尺寸的专用 footprint 与两个边缘位置 fit-check 均通过；
  `H035=3.50 mm` 已按 Y 前后总深度建模，10 个 STEP 有效、修正后硬碰撞仍为 0，
  但按钮帽、手感、后壳和完整 Z-stack 未验证，`selection_frozen=false`。
- L1 原理图修改合同：参考 77 个器件/102 个网络完整分区，47/47 自检 PASS。
- L1 dry-run 调查：`analysis/L1_SCHEMATIC_DRYRUN_FEASIBILITY.md` 确认可在完整隔离副本
  中实施；使用 KiCad 10 GUI 与 ERC 差分，不能把导入型 ERC 强行清零。
- L1 隔离副本编辑前基线：KiCad 10.0.4 可重新导出 77 个器件、102 个网络、691 项
  导入型 ERC 和一页 A3 PDF；PDF 已渲染目视检查。该结果只证明副本完整可读。
- L1 候选网表门禁：11 个 synthetic 正反 fixture 自测通过；正式两页 KiCad 原理图候选
  又以真实 KiCad XML 网表通过 20/20，`real_candidate_validated=true`。这只证明原理图
  结构/网络；`pcb_synced=false`、`fpc_selection_frozen=false`、`production_ready=false`。
- ERC error 稳定差分：导入基线 18 → 正式候选 14，新增 0、旧显示删除带走 4；
  585 个 warning 和 14 个继承 error 仍需按风险处置，不能称为生产 ERC 通过。
- PCB 同步前置检查：新增 26 个器件已有 24 个 footprint；H1/Q1 丢失元数据已按
  参考 PCB 精确恢复，只剩 `J_DISP1` 与 `SW_PWR1` 必须等实物冻结。
- L2 输入、Power/Lock 和 USB 的 GPIO/器件仍是候选；正式 PCB 尚未建立，因而当前
  不存在可下单 Gerber、生产 BOM 或 CPL。
- 嘉立创桌面端回导副本保留 77 个封装和主要几何，但所有 PCB 网络归属为空；详见
  `roundtrip/easyeda-pro/ROUNDTRIP_AUDIT.md`。15 处 `Arial` 字体告警本身只影响文字，
  网络丢失才是禁止把该 `.eprj2` 当电气主工程的原因。
