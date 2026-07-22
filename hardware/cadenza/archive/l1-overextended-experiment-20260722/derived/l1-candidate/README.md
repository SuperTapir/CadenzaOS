# Cadenza L1 原理图候选

这是从已隔离的参考工程派生出的 **正式原理图候选**，不是参考母版，也不是可直接下单的生产工程。

当前范围：

- 第 1 页保留参考设计的 ESP32-S3、USB、IP5306、电池、音频、按键和其余最小系统。
- 第 2 页新增 Sharp LS027B7DH01 接口、电源/锁屏按键电路和测试点。
- 真实 KiCad XML 网表必须通过 `verify_l1_candidate_netlist.py` 的 20 项合同检查。
- `J_DISP1` 的 footprint 故意留空；真实 FPC 触点面、Pin 1、出线方向和锁扣方向尚未验证。
- `R_PWR6` 的数值仍保留 `0R_HIGH_CURRENT_TBD`，footprint 已选为标准
  `NetTie-2_SMD_Pad2.0mm` 铜桥候选；13 个测试点使用 1.5 mm 裸铜 pad。两类 footprint
  均排除 BOM/CPL，但仍须在正式 PCB 上验证位置、回流路径和铜颈缩。
- 本目录暂不包含同步后的 PCB。现有参考 PCB 不能与本原理图候选混称为已同步。

状态：`real_candidate_validated=true` 仅表示原理图网表结构已由真实候选验证；`production_ready=false`。
