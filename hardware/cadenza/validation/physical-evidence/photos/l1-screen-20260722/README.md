# LS027B7DH01 屏幕正反面实物证据

拍摄日期：2026-07-22。用户明确标注 `front` 为正面、`back` 为反面；原始 HEIC 与无损方向转换的 PNG 一并保存。

## 已确认

- 显示面朝上时，FPC 从屏幕下边（6 点钟方向）自然伸出。
- 正面照片中尾端金手指被黑色背衬覆盖；反面照片中十条金手指完全可见，因此 FPC 金手指位于屏幕背面。
- FPC 为十条导体，实物外观与项目采用的 10-pin Sharp 屏幕尾线一致。
- 屏幕没有为了拍照而扭转 FPC；正反照片可以建立同一实物的左右对应关系。

## 仍不能仅凭照片确认

- 照片中没有足够清晰且无歧义的数字 `1` 或三角 Pin 1 标记。Pin 1 的“显示面朝上时位于右侧”仍来自 Sharp 数据手册，不冒充为照片独立证明。
- 尚未把 FPC 折到真实 PCB/FH34 连接器中，因此锁扣操作、折弯半径、USB 间隙和折后全局 Pin 1 映射仍待实装。
- 尚未对已购 2.54 mm 转接板做 1-10 连续性测试；该转接板只用于面包板验证，不决定最终 PCB 上 FH34 的引脚编号。

## 下一步断电核对

按 100% / Actual Size 打印 [Cadenza L1 FPC 实物方向检查纸](/Users/tapir/Development/CadenzaOS/output/pdf/cadenza-l1-fpc-physical-fit-sheet-a4.pdf)，先量图中的 50.00 mm 校准线，再把屏幕显示面朝上放到绿色外框中。该纸样只用于确认折弯、Pin 1 对应关系、锁扣操作和 USB 间隙，不允许通电。

## SHA-256

```text
fc40e2c1bf169ce9e2421497427f60b4367554af3d026a87c9f2c04f0a13937d  back.HEIC
5cd84e89fca4fce0f1108437f493ff1438df5b175ffc8e4a731a9153ade188c4  back.png
26cf0989fd7354c0a80671c13de9ff04712fc2ffdedf69d53f39353ca25f4ceb  front.HEIC
099196333607d4a1f2e0583b50f4a7ea3a1a269ab78b404033cfc964c518005c  front.png
```
