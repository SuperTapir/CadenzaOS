# System Overlay 采用决策

本文把 `docs/system-overlay-reference-research.md` 的源码证据收敛为 Cadenza 的采用/不采用边界。状态为：设计与 portable 软件实现已收敛，软件门禁见 `system-overlay-implementation-report.md`，真机交互验收仍待实体设备。

## 采用

1. **System Menu 由系统拥有，不注册成 App。** 长按进入 system presentation state，current AppId 不变，不产生 App exit/enter。
2. **固定 layer 和单 interactive slot。** Base App、transient、interactive、persistent/critical 顺序固定；第一版不提供任意 modal stack。
3. **完整 button sequence owner。** Opening/closing surface 的 Press、LongPress、Release/click 归同一 owner，trigger release 不可确认 Menu，也不可泄漏给 App。
4. **Suspend-with-snapshot。** System Menu active 时 App update/input/render 停止，背景冻结；SystemService frame transaction 继续。
5. **Transition defer。** 普通 Menu 请求发生在 App transition 时先捕获输入，等 stable target frame 再打开。
6. **Typed async action。** Menu 使用 enum model 和现有 SystemCommand；draw 不改变 authoritative state，不执行任意 callback。
7. **Stateless primitives。** Panel、Header、MenuRow、Selection、Value、StatusIndicator 只接收 canvas/bounds/value，不拥有 service、input 或动态树。
8. **受限系统菜单。** 首版只提供 Resume、Sound、Motion 和非 Home 场景下的 Home action；Connectivity/Device 等无动作状态不进入高频菜单；不接受 App 自定义 View。
9. **有界容量与诊断。** Busy、invalid action、capacity、denied owner 使用稳定 rejection 与 counter。

## 不采用

- 不引入 LVGL、Qt Quick、microui 或 Flipper GUI 作为 runtime dependency。
- 不复制 Flipper GPL-3.0 实现。
- 不把 System Menu 做成隐藏 App、App transition 或全局 callback registry。
- 不实现 React/DOM/reconciliation、任意 retained widget tree 或通用 focus engine。
- 不用同步 blocking Dialog API重入当前单线程 frame transaction。
- 不允许 App 删除、重排、伪装系统条目或持有 Menu renderer。
- 不把 Toast、Indicator 与 modal Menu 统一成一个含糊的 `showOverlay(bool)` API。
- 不在第一版承诺 persistence、notification center、installable App 或多 modal priority arbitration。

## 失败即回退条件

若以下任一项无法以自动化或 size/build 证据满足，不得把实现包装为完成：

- 触发 long-press 的 release 会执行 Menu action 或进入 App；
- Menu 关闭时 held sequence 泄漏；
- Menu active 时 App 仍 update/render；
- SystemService 因 Menu 暂停；
- transition 中打开 Menu 导致生命周期、framebuffer 或 current AppId 不确定；
- 需要新增第三个最大 framebuffer；
- 双 profile 出界或改变项目顺序/动作语义；
- core/system 引入平台头、research clone 或新 GUI dependency；
- 普通 App 获得任意 system item/callback 注入能力。

## 后续扩展门槛

- Dialog：出现首个真实危险确认 consumer 后，单独定义 close/result/suspend policy。
- Toast：先证明 fixed queue、expiry、dedupe 和非交互 input 行为。
- App menu contribution：先定义最多数量、descriptor 字段、权限、异步 result 与系统命名隔离，不能直接暴露 View/callback。
- 多输入硬件：若未来不再是单按钮，先把 sequence id/owner 下沉到 reducer，再扩张 overlay router。
