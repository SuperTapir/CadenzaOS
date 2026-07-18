# Launcher 卡片参考调研与采用边界

状态：Playdate 1.12.3 文档与 SDK 示例的源码/资产级调研已完成；Cadenza
的软件实现和 host 视觉基线由 `add-continuous-launcher-navigation` 变更跟踪，实体
T-Embed 的 1-bit 运动观感仍待真机验收。

## 锁定参考

| 参考 | 版本与许可证 | 阅读范围 | 对 Cadenza 的结论 |
| --- | --- | --- | --- |
| Designing for Playdate | SDK 1.12.3，Panic 官方文档 | Launcher、Launcher card、30/20 FPS、1-bit dither 与 flashing | 卡片是 App 的第一识别物；名称进入卡片构图；selected、pressed、loading 与首屏应作为连续体验设计 |
| Inside Playdate | SDK 1.12.3，Panic 官方文档 | System/Game Metadata 的 `card.png`、`card-highlighted/`、`card-pressed.png`、`launchImages/` | 官方固定卡片为 350×155，启动帧为 400×240/20 FPS；状态资产各自保持固定画布，不应随 Launcher 裁切窗口重排 |
| Playdate SDK `Examples/Level 1-1/Source/SystemAssets` | SDK 1.12.3，示例 BSD-0 | card、4 个 highlighted 帧、pressed、9 个 launch 帧和 `animation.txt` | 示例通过同一场景内角色姿态/物件变化表达状态，而不是由 Launcher 统一覆盖粗重控件 |

本项目采用“卡片是 App 第一识别物”和固定画布原则，不采用 Playdate 的 Cover
状态资产分层，也不复制 Panic/示例的像素、美术、字体、角色或品牌。Cadenza
内置 Cover 必须原创且静态，并以固件内嵌的双 profile packed 1-bit bitmap 发布；
运行时 PDI/PNG loader、外部 App bitmap pack、逐帧启动资源描述和安装协议不在本次范围。

## 采用的设计

- App 通过可选的 const、无分配 Cover renderer 表达身份；Launcher 只负责目录、
  轨道、单层边界和 fallback，不向 Cover 传递交互状态。
- 逻辑选择立即响应输入，视觉位置连续追赶无界逻辑目标；循环边界只移动一个
  pitch，连续反向输入从当前视觉位置 retarget。
- 横向和纵向共用同一轴向布局。Cover 始终收到固定完整卡片 bounds，Launcher 用
  显式 viewport clip 裁出屏幕内部分；裁切宽度变化不得触发 Cover 重排。
- Cover 是静态、不可交互的身份图。未选中、选中、按下、长按、启动和 App 自身
  lifecycle 变化不得改变其像素；这条约束直接排除 Clock 时间跳变、Motion 圆形
  变黑以及统一 press 状态条。未来启动动画必须使用独立资源和契约。
- 相邻 Cover 本身承担方向预告，因此不重复绘制 footer 名称或页码圆点。
- Launcher 装饰固定在屏幕坐标；不得让高频纹理跟随卡片逐像素移动。

## 明确不采用

- 不按 `AppId` 在 Launcher 中 switch 绘制内置 Cover；那会把 App 身份重新耦合到
  系统入口。
- 不把部分可见矩形当成新的 Cover 画布；它会让横向移动时标题、圆和曲线每帧缩放
  或重排。
- 不在运行时缩放或裁切当前 Cover 内容。Sharp 使用完整 350×155，T-Embed 使用
  从同一 canonical 画布离线等比生成的 280×124；横纵布局由 Launcher 适配同一
  profile 尺寸。外部 App 的资源加载、缩放和安装契约仍未冻结。
- 不为将来预留 Highlighted/Pressed/Launching Cover 参数；这种预留已经诱发内容
  在点击瞬间变异。需要动态启动体验时单独设计 launch animation。
- 不复制 Playdate 的启动转场实现，也不声称 host 60 Hz 确定性快照证明 Memory
  LCD/TFT 真机观感。

## 实体设备验收

在原版 320×170 T-Embed 上，以 release firmware 和正常握持方式执行：

1. 分别选择 Vertical 与 Horizontal，慢速逐项旋转两轮；当前与相邻 Cover 应保持
   刚性构图，只沿单一轴移动，不得出现内容先跳、缩放或重排。
2. 快速正向 20 步、立即反向 10 步，并在未 settled 时短按；打开对象必须是最新
   逻辑选择，轨道不能排队追赶或跨列表长距离回绕。
3. 在 Normal 与 Reduced Motion 各执行 100 次首尾循环；Reduced 必须连续且无
   overshoot，Normal overshoot 不得露出空白 slot。
4. 用相机录制稳定 30 FPS 目标下的纵向/横向运动，检查最慢帧、撕裂、拖影以及
   逐像素黑白相位闪烁。软件 hash 通过不能替代该步骤。
5. 对 Clock、Motion 分别短按和长按 Confirm，并逐帧对比；Cover 内部像素必须始终
   相同。打开系统菜单再 Resume 后，debug HUD 在无本帧误用时应显示 `DIAG OK`。

在这些步骤有实体证据前，状态必须写为
`automated implementation complete / hardware motion acceptance pending`。
