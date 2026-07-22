# L1 局部布线候选

状态：**受控 Power/Lock 与完整 USB 连接器布线；未完成、未冻结、不可下单。**

本目录从 `../l1-pcb-candidate/` 的两个已核验同步板派生，不修改同步板：

- 新增 51 个铜对象，共 13 组受控走线；
- 涉及三个局部 Power/Lock 网络，以及 USB `CC1/CC2`、`D+/D-`、`VBUS`；
- 两个 FPC 方向均从 56 个未连接降到 38 个；
- Type-C A/B 面的数据脚分别在顶/底层汇合，不增加数据线过孔；
- D+ 新增总长约 15.36 mm，D- 约 13.35 mm，绝对差约 2.01 mm；
- VBUS 保留参考设计的 0.508 mm 线宽，但避开了移动 USB 后的 Power/Lock 测试点；
- 新增走线没有 `shorting_items`、`tracks_crossing` 或其他新增走线高风险 DRC；
- 连续生成两次的 PCB 哈希一致；
- 独立验证 28/28；新增 R_PWR4 gate pulldown 和 R_PWR2 3V3M 下接也未引入高风险 DRC。

尚未布线的重点包括 Sharp 显示信号、Power/Lock 长距离 MCU 控制线、LED、测试点、
高电流 VOUT→R_PWR6，以及 R_PWR4 gate pulldown。USB 的 90 Ω 目标与 VBUS 载流能力
仍须等嘉立创最终叠层和铜厚确定后计算，不能由本阶段门禁代替。

CH340C 的官方版本、引脚与 USB Full-Speed 约束证据见
`../../sourcing/usb-uart/CH340C_USB_UART_CONSTRAINTS.md`。WCH 明确要求 D+/D−直接连接、
不额外串联电阻；当前候选符合该拓扑，但尚未完成叠层阻抗审查。

生成与验证：

```sh
/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
  hardware/cadenza/derived/l1-pcb-routing-candidate/route_l1_pcb_candidate.py

/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 \
  hardware/cadenza/derived/l1-pcb-routing-candidate/verify_local_routing_candidate.py
```

`PASS_LOCAL_ROUTING_VERIFIED` 只证明这批新增走线是受控增量，不代表全板完成或 DRC
通过。当前两块板各有 38 条未连接；其中真实待布网络仍是生产阻断项（报告中也包含
USB 外壳焊盘和 U7 多重接地焊盘等需要逐项分类的连接提示）。
当前 `routing_complete=false`、`drc_passed=false`、`production_ready=false`。
