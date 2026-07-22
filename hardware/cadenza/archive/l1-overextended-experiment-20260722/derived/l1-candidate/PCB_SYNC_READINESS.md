# L1 PCB 同步前置检查

状态：**PASS_BLOCKED_BY_DECLARED_FOOTPRINTS**。这不是 PCB 同步通过，也不是可生产结论。

- 参考 PCB：77 个 footprint。
- 正式候选原理图：94 个实体器件。
- 新增 26 个器件中，24 个已有 footprint，2 个仍需实物选型。
- `pcb_sync_ready=false`，`production_ready=false`。

## 必须等实物

- `J_DISP1`：真实 FPC 触点面、Pin 1、锁扣和出线方向。
- `SW_PWR1`：真实侧按开关料号、焊盘、按压方向和外壳推杆配合。

## 可以先做但尚未实施

- `R_PWR6` 已指定 `NetTie-2_SMD_Pad2.0mm` 短宽铜桥候选；仍须在 PCB 上检查电流回路和铜宽。
- 13 个测试点已指定 `TestPoint_Pad_D1.5mm` 裸铜焊盘；最终 BOM/CPL 必须排除。
- 已修复两项导入缺失的 footprint 元数据：
- `H1`：从参考 PCB 恢复 `Cadenza-re-easyedapro:CONN-SMD_2P-P2.00_XUNPU_WAFER-PH2.0-2PWB`。
- `Q1`：从参考 PCB 恢复 `Cadenza-re-easyedapro:SC-70-6_L2.2-W1.3-P0.65-LS2.1-BR`。

## 边界

参考 PCB 仍只是 77-footprint 的布局来源。本报告没有创建或同步 L1 PCB。
