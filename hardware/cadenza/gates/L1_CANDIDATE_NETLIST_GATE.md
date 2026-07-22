# L1 候选网表合同门禁

状态：**验证器已实现；尚无真实 L1 候选通过记录。**

该门禁在 L1 原理图 dry-run 之前定义成功条件。它以
`l1-schematic-delta-contract.json` 和冻结的
`working-base/connectivity.netlist.xml` 为真值，只读检查候选结构，不修改
KiCad、PCB 或 OpenSpec。

## 使用

直接检查 KiCad XML 网表：

```bash
/Users/tapir/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3 \
  hardware/cadenza/gates/verify_l1_candidate_netlist.py \
  path/to/candidate.netlist.xml \
  --output path/to/result.json
```

也可以传入 `.kicad_sch`。验证器会调用 KiCad CLI，把网表导出到系统临时
目录后检查，临时目录退出时自动删除：

```bash
/Users/tapir/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3 \
  hardware/cadenza/gates/verify_l1_candidate_netlist.py \
  path/to/candidate.kicad_sch
```

退出码 `0` 表示 `PASS_CANDIDATE`，退出码 `1` 表示合同不符合或执行错误。
JSON 始终明确写入 `production_ready: false`、`fpc_selection_frozen: false`。

## 覆盖范围

- 冻结基线原理图/网表哈希；
- 候选真实器件总数 94；
- 9 个删除、1 个局部修改、67 个保留、26 个新增 designator；
- 26 个新增器件的 reference 对应 value；电阻支持 `100R = 100ohm`、
  `0R = 0ohm` 等明确等价写法，显示电容按数值和下限比较；
- 67 个保留器件的 value、footprint 和逐 pin 端点；
- `SW2` 仅 pad 5 允许变成明确未连接；
- `J_DISP1` footprint 必须为空，Sharp pin 1-10 必须逐项精确匹配；
- 三颗显示电容的端点和值；
- Power/Lock 和全部显示/电源测试点端点；
- `D_PWR1` 的 A1/A2/共阴极及 `Q_PWR1` 的 G/S/D pinfunction。KiCad 10
  `D_PWR1` 必须使用 exact `Diode:BAT54C`。KiCad 10 标准
  `Diode:BAT54C` 在 XML 中三个 pinfunction 都为空，因此只对这个 exact
  `Diode:BAT54C` 接受并在 JSON `details.pinfunction_exemptions` 记录豁免依据；
  其他空白/未知失败。`Q_PWR1` 必须是 exact
  `Transistor_FET:AO3400A`，且 G/S/D 明确匹配；
- 禁止 `+5V -> U4 GPIO`、`3V3M -> Sharp pin 6/7/8`，以及未隔离的电源键节点直连 GPIO。

旧 `/GPIO3` 是删除旧 TFT 后唯一会留在保留器件上的删除网络端点；验证器只
允许 `U4` 对应 pin 变成缺失/明确 no-connect，不允许改接其他网络。

## 自测

```bash
/Users/tapir/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3 \
  hardware/cadenza/gates/selftest_l1_candidate_netlist.py
```

九个声明式 fixture 会在临时目录生成 XML：合格 fixture 和明确等价值 fixture
必须通过；Sharp
关键 pin 交换、提前选择 `J_DISP1` footprint、受保护端点漂移、`R_PWR3`
从 100R 误改为 100k、BAT54C pinfunction 错位、BAT54C/Q_PWR1 错误 libsource
必须失败。fixture
仅证明门禁能识别这些情况，**不代表真实候选原理图已经实现或通过**。

KiCad XML 网表没有稳定、跨导出方式一致的 DNP 装配语义，因此本门禁不伪造
`R_PWR5 = DNP` 检查。该项必须在后续 BOM/装配属性门禁中核对。

## 不证明什么

通过不代表 ERC、PCB、FPC 实物方向、DFM、EMC、BOM 或生产就绪。真实候选仍需
由 KiCad 10 重新载入并导出网表，另做 ERC 差分、PDF/SVG 人工检查和源文件哈希
保护；FPC footprint 继续等待实物触点面、Pin 1 和连续性证据。
