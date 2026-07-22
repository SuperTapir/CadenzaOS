# L1 PCB 变更边界门禁

这个门禁比较原始参考 PCB 与将来的 L1 候选 PCB，目标是证明改动仍然只是“更换显示屏”，而不是悄悄重画整板。

运行：

```bash
/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
  tools/verify_screen_only_pcb_boundary.py
```

状态和退出码：

- `PASS` / `0`：所有边界检查通过；
- `FAIL` / `1`：候选存在，但至少一项越界或连通性检查失败；
- `PENDING` / `2`：候选 PCB 尚不存在。此状态不会伪装成通过。

门禁冻结以下内容：

- 只删除 `FPC1/U6/Q2/R13/R14`，只新增 `J_DISP1/C_DISP1`；
- 除 `C20/C21` 外，其余 70 个保留元件的位置、旋转、正反面、footprint ID 和 value 不变；
- `C20/C21` 只允许移动、旋转，并把原显示电源端改接 `+5V`；
- 板框、四个 M2 安装孔、`U4` 和 `USB1` 的物理几何不变，`USB1` 引脚网络也不变；
- `J_DISP1` 必须使用已选 FH34SRJ footprint，并按 Sharp pin 1–10 映射；
- `C_DISP1` 必须是 `GPIO12` 到 `GND` 的 100 nF 电容；
- 旧 `3V3LCD/GPIO3/$1N34531/$1N34538` 不得残留在 netinfo、焊盘、走线、过孔或铺铜中；
- 每次都用 pcbnew 重建连通性并读取网络、节点、焊盘、走线、过孔、铺铜和 unconnected 数量；候选必须是 `0 unconnected`。

如需给自动化流程读取完整证据，可增加 `--json`。门禁只证明修改边界和 PCB 内部连通状态，不代替 DRC、差分 USB 检查、FPC 实物方向确认、Gerber/钻孔检查或嘉立创回导检查。
