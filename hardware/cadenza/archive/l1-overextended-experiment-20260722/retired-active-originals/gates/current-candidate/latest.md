# Current Hardware Candidate Gate

Verdict: **PASS_CANDIDATE**

Release class: **candidate only / not production ready**

This gate reruns the current candidate evidence. It does not certify a JLCPCB order,
freeze an FPC direction, or replace physical fit and bring-up tests.

| Result | Check | Scope |
|---|---|---|
| PASS | `display_baseline` | L1 display mapping/domain/GPIO candidate |
| PASS | `fpc_library` | 10-pin FPC candidate footprint geometry and Pin 1 |
| PASS | `l1_fpc_variants` | L1 four-direction/contact FPC fit variants |
| PASS | `l1_fit_audit` | L1 reference-envelope/window/holes/collision audit |
| PASS | `l2_input_spice` | L2 button/encoder RC tolerance-corner SPICE |
| PASS | `l2_mechanical` | L2 B/Menu/encoder+A mechanical candidates |
| PASS | `l1_schematic_delta_contract` | L1 schematic delta contract against frozen baseline |
| PASS | `l1_pre_edit_baseline` | L1 pre-edit dry-run baseline proof (not implementation) |
| PASS | `l1_candidate_netlist_selftest` | L1 candidate-netlist validator synthetic self-test (not a real candidate) |
| PASS | `l1_real_candidate_netlist` | L1 real two-page KiCad schematic candidate netlist contract |
| PASS | `l1_erc_delta` | L1 candidate adds no new error-level ERC signatures |
| PASS | `l1_pcb_sync_readiness` | L1 inherited-board to real-schematic footprint sync preflight |
| PASS | `l1_pcb_only_footprints` | L1 net-tie and bare-pad testpoint footprint geometry |
| PASS | `l1_pcb_placement` | L1 FPC/USB/Power planar placement feasibility |
| PASS | `l1_pcb_sync_candidate` | L1 two-variant PCB netlist/mechanical synchronization audit |
| PASS | `l1_local_routing_candidate` | L1 controlled local Power/Lock routing delta audit |
| PASS | `l1_risk_register` | L1 printable enclosure and assembly non-frozen risk register |
| PASS | `l2_printable_controls` | L2 printable keycaps/knobs and 3×3 assemblies |
| PASS | `power_lock_mechanical` | Power/Lock mechanical candidates |
| PASS | `s1_power_lock_footprint` | S1 datasheet-dimensioned footprint and axis semantics |
| PASS | `s1_power_lock_fit` | S1 real-dimension A/B enclosure fit candidates |
| PASS | `physical_fit_sheets` | A4 1:1 connector/caliper fit sheets |
| PASS | `l2_input_host_c` | L2 input logic host C tests with warnings as errors |
| PASS | `reference_connectivity` | Reference schematic-to-PCB endpoint topology |
| PASS | `openspec_strict` | OpenSpec change strict validation |
| PASS | `protected_inputs_unchanged` | Protected KiCad/reference/OpenSpec/candidate inputs unchanged |

## Key evidence

- Reference connectivity: `58/58` PCB nets exactly match schematic endpoint topology.
- L1 FPC direction variants: `4`; selection frozen: `false`.
- L1 maximum hard collision volume: `0.0` mm³.
- L2 input SPICE corners: `fast, nominal, slow`.
- L2 mechanical selection frozen: `false`.
- L1 schematic delta contract: `47/47` checks; FPC frozen: `false`.
- L1 pre-edit dry-run baseline: `8/8` checks and `39` manifest files; L1 implemented: `false`.
- L1 candidate-netlist validator self-test: `11/11` synthetic fixtures; real candidate validated: `false`.
- L1 real hierarchical schematic candidate: `20/20` contract checks; real candidate validated: `true`; PCB synced: `true`.
- L1 ERC error regression: baseline `18` → candidate `14`; new errors: `0`.
- L1 PCB sync preflight: `24/26` new components have footprints; physical footprint blockers: `['J_DISP1', 'SW_PWR1']`; PCB synced: `false`.
- L1 PCB-only footprints: `PASS_CANDIDATE`; nominal 2.1 A net-tie drop: `2.069` mV; layout validated: `false`.
- L1 placement feasibility: `PASS_PLACEMENT_CANDIDATE`; FPC-to-shifted-USB gap: `5.677` mm; USB-to-LED1 gap: `1.068` mm; PCB created: `true`.
- L1 PCB synchronization candidate: `PASS_SYNC_VERIFIED` with `33/33` checks; footprints: `94`; named nets: `97`; routed/DRC/production ready: `false/false/false`.
- L1 local routing candidate: `PASS_LOCAL_ROUTING_VERIFIED` with `28/28` checks; added track segments: `46`; unconnected: `56` → `40`; routing complete/production ready: `false/false`.
- L1 print/assembly risk register: `PASS_NON_FROZEN_AUDIT` with `9` pending risks; selection frozen: `false`.
- L2 printable controls: `21` STEP + `21` STL, `3×3` candidates; selection frozen: `false`; production ready: `false`.
- Physical fit sheets: `4` PDF pages and `3` connector footprints; orientation frozen: `false`.
- L2 input host C: `PASS` with `-Wall -Wextra -Werror`; ESP-IDF full build: `PASS_RECORDED_NOT_RERUN` (recorded only, daily gate: `false`).
- S1 Power/Lock footprint: `PASS`; axes verified: `true`; selection frozen: `false`.
- S1 Power/Lock fit: `PASS` with `10` valid STEP files; selection frozen: `false`.
- Power/Lock production ready: `false`.
- Protected KiCad/reference/OpenSpec inputs unchanged during the run: `true`.

## Selected runtimes

- Python: `/Users/tapir/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3` (3.12)
- CAD/OCP: `/Users/tapir/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3` with `PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools`
- Printable-controls CAD/OCP: `/Users/tapir/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3` with `PYTHONPATH=/Users/tapir/.cache/cadenza-cad-tools`
- Bundled PDF Python: `/Users/tapir/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/bin/python3` (pypdf `6.10.0`, reportlab `4.4.9`)
- Host C compiler: `/usr/bin/cc`
- KiCad pcbnew: `/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3` (10.0.4)
- ngspice: `/opt/homebrew/bin/ngspice`
- OpenSpec: `/opt/homebrew/bin/openspec`

## Pending before any freeze/order

- 用真实 10-pin FPC/转接板确认触点面、Pin 1 与自然出线方向。
- 用真实 LS027B7DH01 做无应力装配、窗口无遮挡、FPC 不受拉力和完整闭壳测试。
- 实测 EC12/按键尺寸、每格脉冲和接点抖动；当前 SPICE 只证明候选 RC 模型。
- 冻结 Power/Lock 开关料号、封装、按帽配合，并验证短按锁屏/长按硬关机行为。
- 完成已同步 L1 PCB 候选的布局清理和 49 个未连接项布线，再运行 DRC、交叉分析、EMC、DFM、BOM/CPL/Gerber 门禁。
- 打印 L2 输入候选并完成手感、可达性、旋钮尺寸及完整外壳闭合验证。

## Safety boundary

- Validators that normally write local results ran in a `mktemp` mirror.
- `golden-import/`, `working-base/`, both reference enclosure STEP files, OpenSpec change, the 39-file pre-edit source manifest/dry-run, the real L1 schematic, PCB synchronization and controlled local-routing candidates, R_PWR6 sourcing evidence, S1 Power/Lock datasheet/footprint/fit candidates, netlist validator/self-test/11 fixtures, delta contract, printable controls, fit sheets/PDF, and L2 firmware sources were hash-checked before and after.
- A PASS here means the named candidate checks are reproducible now. It is **not** a production-ready claim.
