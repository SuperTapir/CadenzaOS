# L1 fit-check 可重复性审计

> 结果：**PASS_NON_FROZEN_AUDIT**。该结果只证明当前非冻结 fit-check 可重复生成，不表示外壳可直接打印或 PCB 可打板。

- 当前 PCB SHA-256：`8aa99d03a21dfcb8926d6cdb0cf773a8a9d5a99e0044450145ebda28027e093d`。
- Sharp 面板/窗口相对旧居中位置在外壳坐标均移动 `+1.5 mm`；对应 KiCad 为 `X +1.5 mm / Y -1.5 mm`。
- J_DISP1 位于 KiCad `(154.625, 128.500)`；USB1 固定于 `(153.125, 136.121)`；两者 courtyard 的 Y 向净距为 `0.965567 mm`。
- 声明的代理 Z 下硬碰撞最大值为 `0.000000 mm³`，但 FPC 折弯 keepout 与 FH34/USB1 都有交叠，最小交叠 `13.693050 mm³`。
- 74 个 footprint 中 66 个有 F.CrtYd；其中 31 个 courtyard 的 XY 投影落在 Sharp 玻璃范围内。没有真实高度与 PCB 到前壳 Z 实测，不能据此清除闭壳风险。
- 生成并校验 `11` 个 STEP；缺失输出 `0` 个。

实物门槛仍包括 FPC 出线/触点/Pin 1、180° 折弯路线、FH34 锁扣操作、USB1 真实高度、PCB 安装 Z 和通电闭壳测试。
