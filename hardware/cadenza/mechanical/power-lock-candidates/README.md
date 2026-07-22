# Power/Lock 机械候选（未冻结）

这里探索参考掌机改造后的独立 Power/Lock 按钮位置。它只生成派生候选，不修改 L1 主外壳、KiCad 或 OpenSpec。

## 共享基准和两种位置

- PCB：`130 × 60 mm`；四孔仍为 `(5,5)`、`(5,55)`、`(125,5)`、`(125,55)`。
- Sharp 面板、FPC 避让及三套 L2 输入代理均作为碰撞边界。
- A 顶边：按钮轴线在 `X=105 mm`，从顶边向外按；它避开中间 FPC 和右上安装孔。
- B 右侧边：按钮轴线在 `Y=18 mm`，从右侧向外按；它与右侧编码器候选错开。

两套都有按帽、圆筒导向、两侧防误触护栏形成的凹入触区、开关本体与完整按压行程 keepout，以及包含 L1 前壳的组合 STEP/STL 和 PNG 预览。护栏比静止按帽外端多突出 `0.8 mm`，因此平面擦碰不容易直接压下按钮。

这不代表已经选定安装位置。A 更容易从顶边找到，但可能影响顶边接口布局；B 更隐蔽，但握持时误触和右手可达性需要实体手板验证。

## 不能用于生产的原因

当前按帽 Ø5.6 mm、假设行程 1.0 mm、开关本体 `6.6 × 6.6 × 5.4 mm` 和板壳相对 Z 高度都只是参数化代理。以下资料仍是**待验证**：

1. 真实常开瞬时开关型号、LCSC 编号、封装和库存等级；
2. 本体、执行头、焊脚、定位柱和端子禁布尺寸；
3. 总行程、操作力、寿命、侧向负载能力；
4. PCB 元件面到完整顶/侧壁的装配距离；
5. 后壳、分模线、螺柱、接口和装配顺序；
6. 打印材料收缩、按帽配合、导向间隙和实机误触；
7. 电气 Power/Lock 短按、长按与硬件关机行为。

因此 STEP/STL 只能用于候选查看或低成本手感样，不能提交最终 3D 打印，更不能据此冻结 PCB footprint。

## 输出

每个 `generated/<variant>/` 包含：

- `*-enclosure-interface.step/.stl`：L1 前壳加局部边墙、开孔、导向和护栏的候选接口；
- `*-button-cap-proxy.step/.stl`：按帽包络；
- `*-guide-proxy.step/.stl`：带径向间隙的圆筒导向；
- `*-guarded-recess-proxy.step/.stl`：防误触护栏/凹入触区；
- `*-switch-keepout-proxy.step/.stl`：开关本体与按压扫掠包络；
- `*-assembly.step/.stl`、`*-assembly-preview.png` 与由组合 STL 实际渲染的 `*-assembly-iso.png`：组合预览；
- `*-parameters.json`：坐标、代理尺寸、间隙和碰撞结果。

汇总见 `generated/power-lock-candidates.json` 和 `COLLISION_REPORT.md`。

## 重复生成和验证

```sh
PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools \
  /Users/tapir/.local/share/uv/python/cpython-3.12.11-macos-aarch64-none/bin/python3.12 \
  hardware/cadenza/mechanical/power-lock-candidates/generate_power_lock_candidates.py

/Users/tapir/.local/share/uv/python/cpython-3.12.11-macos-aarch64-none/bin/python3.12 \
  hardware/cadenza/mechanical/power-lock-candidates/verify_power_lock_candidates.py
```

验证器要求：两个候选和全部 34 项几何/预览/参数输出存在，生产状态保持 `false`，指定硬碰撞体积为零，屏幕与 FPC 的二维间隙至少 3 mm，护栏高出按帽至少 0.5 mm。
