# CH340C USB-UART 证据包

本目录为当前 L1 候选中的 `U3 = CH340C` 提供只读、可审计的厂家证据与布线约束，不修改或冻结原理图、PCB。

- `CH340C_USB_UART_CONSTRAINTS.md`：厂家明确要求、保守工程推导、待叠层计算三类结论。
- `ch340c_extraction.json`：机器可读的引脚、速度、外围和验证状态。
- `SOURCE_MANIFEST.json`：来源 URL、版本、日期、文件角色与 SHA-256。
- `wch-ch340-product-api.json`：WCH 官方产品页 API 原始快照。
- `wch-ch340ds1-file-api.json`：WCH 官方下载元数据 API 原始快照。
- `reference-ch340c-jlcsearch.json`：参考项目现有 jlcsearch 快照的只读副本。
- `CH340DS1-V3C-lcsc-mirror.pdf`：LCSC 托管、正文标注 WCH 的英文 V3C 数据手册副本。

WCH 官方 API 已锁定当前手册为 `CH340DS1.PDF v3.4`、上传日期 `2025-03-03`。官网文件下载接口对本次无交互请求返回刷新提示，因此目录内 PDF **不是官网直下文件**，而是 LCSC 托管的 WCH V3C 英文副本。它用于逐页提取；版本锁定以 WCH 官方 API 元数据为准。

状态：`evidence_only=true`、`routing_frozen=false`、`stackup_calculated=false`、`production_ready=false`。
