# System service architecture reference research

日期：2026-07-18
适用 change：`refactor-system-services-foundation`

## 决策摘要

Cadenza 采用“平台 callback 有界入队 → 单一 system loop 摄取 → App 读取稳定 snapshot 并提交 typed command”的模型，不采用全局同步/异步 event bus。所有容量、delivery、drop、commit 顺序和拒绝都必须在调用边界可见。

## 固定参考与关键源码

| 项目 | Commit / 许可证 | 直接阅读文件 | 有效证据 | 采用 / 不采用 |
|---|---|---|---|---|
| Zephyr zbus | `da9b7b9d257b71098ed2c0d156d137420ea86779` / Apache-2.0 | `include/zephyr/zbus/zbus.h`、`subsys/zbus/zbus.c`、`subsys/zbus/zbus_runtime_observers.c`、`subsys/zbus/Kconfig` | typed channel 和静态 observer 可低成本广播；listener/VDED 可在 publisher context 执行，subscriber 可能只看到被覆盖后的最新 channel state，message subscriber 才复制消息，delivery 语义取决于 observer 类型 | 采用 typed payload、静态容量、显式 delivery mode；不采用全局 channel namespace、publisher-thread listener 和隐式 latest-value delivery |
| Matter/ConnectedHomeIP | `8d8de11a5e84415237bfd42a8fe87cc448d3de62` / Apache-2.0 | `src/include/platform/PlatformManager.h`、`CHIPDeviceEvent.h`、`ConnectivityManager.h`、`internal/GenericPlatformManagerImpl_FreeRTOS.ipp`、`src/platform/ESP32/PlatformManagerImpl.cpp`、`ConnectivityManagerImpl_WiFi.cpp` | platform callbacks 把 `ChipDeviceEvent` 交给系统 event loop；`PostEvent`/`ScheduleWork` 负责线程边界，connectivity manager 在 system thread 推进状态 | 采用 callback marshaling、集中状态机和 thread-safe schedule boundary；不引入完整 Matter stack、全局 singleton 或其动态产品模型 |
| Pigweed `pw_async2` | `1cc1c28fc56f6f65d7d0bcd6945d522d1b9f10e0` / Apache-2.0 | `pw_async2/public/pw_async2/channel.h`、`pw_async2/channels.rst`、`channel_thread_test.cc` | channel 是固定 capacity、多生产/多消费可协调的显式 backpressure；dispatcher 和 simulated time 便于确定性测试 | 采用 bounded channel、backpressure、simulated time 原则；当前不引入 coroutine/dispatcher/toolchain 依赖 |
| Embedded Template Library | `7f290365a9f1b72bf89b4b77ec7dbe39b228bd8e`（20.48.1）/ MIT | `include/etl/message.h`、`message_bus.h`、`message_router.h`、`message_router_registry.h` | 固定容量 router/bus 适合无 heap 嵌入式，broadcast 是同步调用链 | 采用值类型消息和固定容量；不采用 type-erased global bus、同步 fan-out 和难追踪的 router graph |

## 为什么不用一个通用 event bus

通用 bus 会把至少四种不同语义压到同一个 API：

1. App 对 authoritative state 的意图；
2. 平台 callback 的跨线程通知；
3. 每帧稳定、允许丢旧值的状态；
4. PCM 等必须显式 backpressure 的实时流。

Zephyr 的 observer 类型差异说明“publish 成功”不等于“每个消费者得到该 message”；同步 listener 又会把 service 执行带回 publisher 栈。ETL 的同步 routing 对简单 MCU 控制流很好，但若 App、USB 和 Wi-Fi/BLE callback 共用，会产生重入、优先级反转和难以定位的顺序依赖。Cadenza 因此分别使用：

- typed `SystemCommand` FIFO：App intent；
- adapter-owned bounded ingress：跨线程 callback；
- immutable `SystemSnapshot`：App-visible state；
- per-consumer SPSC queue：实时 PCM。

## Frame transaction

```text
adapter ingress
    ↓ bounded ingest
advance services → freeze update snapshot
                         ↓
                    App update
                         ↓ typed FIFO commands
                    commit services
                         ↓
                 freeze render snapshot → App render
```

这一顺序让 Settings 的新音量可以同帧显示，但 update 不能重入观察刚提交的写入。所有 queue 满、budget 用尽、服务 unavailable 和 invalid command 都产生稳定拒绝与 counters，而不是 assertion、阻塞或无限 drain。

## 失败模式与验证

- publisher thread 重入 → callback non-reentrancy trace test；
- latest-state 覆盖事件 → state 使用 snapshot，必须逐条处理的 event 使用 FIFO 并测试 overflow；
- 慢 consumer 阻塞生产者 → PCM 每 consumer 独立 SPSC + drop-newest；
- 单帧事件风暴饿死 UI → 固定 per-frame budget + high-water/reject counter；
- 引入系统服务后 core 反向依赖平台 → core-alone link 与 shared-source audit。

原始参考 clone 只用于阅读，不作为构建依赖。正式采用第三方源码前必须另做 vendoring、notice 和版本更新策略。
