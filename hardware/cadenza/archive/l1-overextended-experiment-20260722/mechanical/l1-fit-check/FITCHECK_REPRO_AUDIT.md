# L1 外壳 fit-check 只读复查

范围：复查已有 L1 前壳、旧四接点观察候选与两套数据手册约束后的外壳旋转候选；不选择外壳旋转或转接板映射，不修改主 KiCad，不更新 OpenSpec 4.2/4.3。

## 判定

结果：**PASS，仍为 non-frozen fit-check。**

- 冻结参考 `顶盖V7.step` 的 SHA-256 仍为 `41a991f288e6c38a120f876404848cc09c7c30d14df6e56ff053dea67efe9bee`。
- 生成前壳与参考顶盖的 XY 外包络一致（允许 STEP 内核 `0.001 mm` 数值容差）。
- 四个 Ø3 mm PCB 固定孔仍位于 `(5,5)`、`(5,55)`、`(125,5)`、`(125,55)`；检查依据是生成 STEP 中四个半径约 1.5 mm 的真实圆柱面，不是参数声明。
- Sharp 窗口是 `60.00 × 36.48 mm`；58.8 × 35.28 mm 可视区每边保留 0.60 mm。
- 0.40 mm 软垫代理、1.00 mm 后压框、FPC relief 和组合 STEP/STL 均存在。
- `+Y/-Y × top/bottom-contact` 四套旧观察候选，以及 `datasheet-plus-y/minus-y-folded-top-contact` 两套外壳旋转候选均存在；没有选定旋转。
- Sharp 规格书 PDF 第 26 页（规格页 23）已确认屏幕局部事实：6 点钟出线、平直触点在背面、正视 Pin 1 在右侧、只能向背面折。
- 两套新增候选的 9.1×6.2×2.0 mm 连接器包络位于 PCB XY 内，和前壳/玻璃/软垫/压框/FPC/PCB 代理的实体硬碰撞为 `0 mm³`。
- 连接器与保守弯折 keepout 仍有约 `28.07136 mm³` 相交，明确保持 `PENDING_EXACT_FOLD_ROUTE_AND_LATCH`，不能误当作可装配证明。
- 共 18 个独立 STEP（基础 4 个、旧方向候选/压框/避让 8 个、两套新增候选各 3 个）均能被 OpenCascade 读取并通过 BREP 有效性检查；配套 STL 齐全。

机器可读证据见 `L1_FITCHECK_AUDIT.json`；独立复查器为 `audit_l1_fitcheck.py`。

## 可重复性

完整流程已连续重跑两次：

```sh
PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools \
  /Users/tapir/.local/share/uv/python/cpython-3.12.11-macos-aarch64-none/bin/python3.12 \
  hardware/cadenza/mechanical/l1-fit-check/generate_l1_front_fitcheck.py

PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools-py313 \
  /opt/homebrew/bin/python3.13 \
  hardware/cadenza/mechanical/l1-fit-check/variants/generate_fpc_direction_variants.py

PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools-py313 \
  /opt/homebrew/bin/python3.13 \
  hardware/cadenza/mechanical/l1-fit-check/variants/verify_fpc_direction_variants.py

PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools-py313 \
  /opt/homebrew/bin/python3.13 \
  hardware/cadenza/mechanical/l1-fit-check/audit_l1_fitcheck.py

PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools-py313 \
  /opt/homebrew/bin/python3.13 \
  hardware/cadenza/mechanical/l1-fit-check/generate_l1_risk_register.py
```

两次运行的语义签名均为：

```text
dabebec857af1b02c8bf6606a8b1f29e24625e07076d7115006e873b893f9057
```

2026-07-22 最新复跑还逐字节核对了全部 18 个 STL 的稳定 bundle hash；连续两次均为：

```text
STL bundle: 6dd50ed6d85807a629cab4bef9ebe4e33d334fa72196855a8b23d32812e59e29
风险 JSON: ce709b1d64a84fad3038804ae24e8c40075c2af9e01cd070edb303f4c3d6ea7e
```

STEP 导出包含生成时间戳，故不把 STEP 文件的原始字节 hash 当作几何一致性证据；STEP 使用上述语义签名、包络/孔位/碰撞结果和 18/18 BREP 有效性检查。

签名覆盖参考哈希、前壳/参考包络、四孔位置、窗口/屏幕/软垫/压框参数、四候选集合、碰撞最大值和有效 STEP 数量；不使用含导出时间戳的 STEP 文件字节哈希作为几何可重复性判据。

## 尚未证明

- 屏幕在外壳中采用 0° 还是 180°，即数据手册 6 点钟边映射 `+Y` 还是 `-Y`。
- 已购屏幕和转接板是否与数据手册 Pin 1/触点面一致，以及转接板是否镜像或非 1:1。
- 精确连接器本体、锁扣打开空间、FPC 自然弯曲路线和合壳余量。
- 软垫压缩量、玻璃应力、打印收缩和压框固定方式。

因此本报告不能用于勾选 OpenSpec 4.2/4.3，也不能把任何候选宣称为可直接打印的最终外壳。

打印/装配风险的补充审计见 `PRINT_ASSEMBLY_RISK_REGISTER.md`和 `L1_PRINT_ASSEMBLY_RISKS.json`；其状态必须保持 `PASS_NON_FROZEN_AUDIT`、`production_ready=false`、`selection_frozen=false`。
