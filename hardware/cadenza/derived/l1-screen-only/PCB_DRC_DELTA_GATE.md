# L1 PCB DRC 增量门禁

参考工程由 EasyEDA 导入 KiCad，原始 PCB 本身带有历史 DRC 项，因此 L1 不把“原始总数必须为 0”作为伪造的成功条件。

运行：

```sh
python3 tools/verify_screen_only_pcb_drc_delta.py
```

门禁同时要求：

- 候选 PCB 为 0 unconnected；
- 候选 DRC 总数不高于参考基线；
- L1 实际修改窗口内没有新增 DRC 项；
- `J_DISP1`、`C_DISP1` 不出现在任何 DRC 项中；
- 短路、交叉、孔间距、线宽、过孔尺寸和阻焊桥等关键类型的数量不高于基线。

输出为 `generated/evidence/pcb-drc-delta.json`。PASS 只证明屏幕增量没有引入新的局部 DRC 问题；原始报告中仍存在的导入债务不会因此被豁免，也不能据此直接宣称可下单。
