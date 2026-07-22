# L1 原理图候选证据

生成日期：2026-07-22

## 结论

- 真实 KiCad XML 网表合同：`20/20 PASS_CANDIDATE`。
- 两次独立生成的根页与子页逐字节一致。
- PDF：2 页 A3，根页 `1/2`，Sharp + Power/Lock 子页 `2/2`；已逐页渲染检查。
- 参考 working-base、golden-import 和 39 文件 pre-edit 基线未被写入或污染。
- `real_candidate_validated=true` 仅适用于原理图结构和网络。
- `pcb_synced=false`，`fpc_selection_frozen=false`，`production_ready=false`。

## 固定摘要

- 实体器件：94。
- 参考设计：9 个旧显示器件删除、67 个无关器件保留、SW2 仅 pad 5 变更。
- 新增：26 个器件，只存在于第 2 页。
- 根页：8 个全局网络锚点，只连接参考页上的 ESP32-S3/IP5306 端点。
- Sharp J_DISP1：1–10 pin 网络逐项通过合同；footprint 仍故意留空。

## 文件哈希

- `Cadenza-L1.kicad_sch`: `6f2bac37516f9133970a4d8e9ec0eed5051e24cc461b7d215ab5cdb3dae8856c`
- `Cadenza-L1-Sharp-Power.kicad_sch`: `e9d0cee6fdec4698a5df969944ae612efa2e5c1ccbcc0c50856408d0f54e2425`
- `candidate.netlist.xml`: `40a5f97f77034897099e21b31176b486de2ea824a5a7987637af299a406d32db`
- `Cadenza-L1-schematic.pdf`: `fac3fd2bcfb23104cd44d73e62317377a28a2e1809310f9f64b3a0e3ee45728c`

## ERC 边界

KiCad 10 ERC 当前报告 599 项：585 warning、14 error。参考导入基线原有 691 项，
其中 18 error。按 violation type + item UUID 做稳定差分后，候选新增 error 为 0、随旧显示
电路删除而消失 4、继承 14。大量 warning 仍是 EasyEDA 导入造成的 off-grid、pin-to-pin
和 library mismatch 债务。这里不以“ERC 清零”包装可靠性；14 个继承 error 仍要在 PCB
阶段逐项判断，但不再误称为 L1 新增回归。

## 未完成

- 未选择真实 FPC 座，因此不能开始冻结屏幕连接器 PCB footprint/方向。
- `SW_PWR1` 的真实侧按开关 footprint/按压方向尚未冻结。
- `R_PWR6` 已指定 KiCad 标准 4 × 2 mm 顶层铜 net-tie 候选，13 个测试点已指定
  1.5 mm 裸铜 pad；它们通过几何检查，但尚未经过最终 PCB 布局/电流回路验证。
- 没有同步 PCB、没有 DRC、没有 Gerber/BOM/CPL，也没有实物验证。
