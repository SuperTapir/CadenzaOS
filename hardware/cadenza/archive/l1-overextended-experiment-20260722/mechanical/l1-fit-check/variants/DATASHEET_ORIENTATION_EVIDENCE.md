# LS027B7DH01 FPC 方向证据

状态：**屏幕局部方向可确认；外壳旋转、转接板映射和折叠装配未冻结。**

唯一权威来源是本地 Sharp 原厂规格书：

- 文件：`hardware/reference/oshwhub-project_jofcnupz/datasheets/LS027B7DH01_Sharp_LCY-1210401A.pdf`
- SHA-256：`997ecbcb7f020fbe077632dc90f6bf82d9e0e49ccf2211484093dec348965d98`
- PDF 第 26 页，规格书页码 23，Figure 8-1 / Figure 8-2。

## 可以从原厂图冻结的屏幕局部事实

| 事实 | 原厂图证据 | 置信度 |
|---|---|---|
| 自然出线边 | Figure 8-1 的 `Display side up` 正视图明确画出 FPC 从屏幕 6 点钟/下边伸出 | 高 |
| 平直触点面 | 正视图显示 P1 stiffener，`Back side up` 视图显示裸露接点；同页推荐 bottom-side-contact 连接器 | 高 |
| Pin 1 | `Display side up` 视图尾端右侧标 `1`、左侧标 `10`；背面视图镜像为左 `1`、右 `10` | 高 |
| 合法弯折方向 | Figure 8-2 和 Remark 8-1 要求向背面折，禁止朝 front polarizer；距玻璃边 0.8–6.0 mm，最小内弯 R0.45 | 高 |
| 折后接点类型 | 同页在弯折说明后推荐 SMK CFP-4510-0150F top-side-contact | 高 |

`P1 stiffener` 是加强板材料/结构标注，不当作 Pin 1 标记；Pin 1 结论来自尾端两侧明确的 `1/10` 数字。

## 仍不能冻结的外壳事实

- 屏幕在 Cadenza 外壳中采用 0° 还是 180°：即数据手册 6 点钟边映射到 PCB `+Y` 或 `-Y`。
- 已购转接板是否镜像编号、插座是上接还是下接、十根线是否 1:1。
- 最终连接器 MPN、锁扣开合方向、焊盘和 PCB Z 高度。
- FPC 的真实自然折线、应力、合壳余量及最多三次折弯的装配流程。

因此目前只可冻结“屏幕自身的局部坐标事实”，不能冻结 PCB 上连接器的旋转、Pin 1 全局坐标或外壳压框最终开槽。
