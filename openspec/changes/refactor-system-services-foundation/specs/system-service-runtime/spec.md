## ADDED Requirements

### Requirement: 系统服务使用类型化命令与稳定快照
系统 SHALL 让 App 只通过封闭的类型化系统命令请求状态变化，并 SHALL 通过只读 `SystemSnapshot` 暴露 authoritative state；App 不得直接持有系统服务、平台 driver 或异步 callback。

#### Scenario: App 请求改变设置
- **WHEN** active App 在 update 中提交有效的设置命令
- **THEN** 命令进入固定容量队列，App 不能在提交调用栈中重入系统服务

#### Scenario: App 读取系统状态
- **WHEN** App update 或 render 查询音量、motion 或 voice 状态
- **THEN** 读取来自该阶段冻结的 `SystemSnapshot`，而不是可并发改变的服务对象

### Requirement: 每帧系统服务顺序确定
系统 SHALL 按“摄取平台事件并冻结快照、App update、FIFO commit App 命令、使用 commit 后快照 render”的顺序执行每帧，并 SHALL 由单一 frame coordinator 保证该顺序。

#### Scenario: 设置在同帧显示
- **WHEN** Settings 在 update 中提交音量切换命令且命令被接受
- **THEN** 同一帧 render 读取 commit 后的新音量，但该次 update 读取的仍是提交前冻结值

#### Scenario: 多条命令有序执行
- **WHEN** 一个 frame 提交多条可接受命令
- **THEN** 系统按提交 FIFO 顺序执行并产生可重复的最终快照

### Requirement: 平台回调被摄取而不直接进入 App
Wi-Fi、BLE、I²S、USB 和其他平台异步 callback SHALL 只写入其 adapter 所有的线程安全有界边界，并 SHALL 由 system loop 摄取为类型化平台事件；callback 不得调用 App 生命周期或 system command commit。

#### Scenario: 后台 callback 到达
- **WHEN** 平台 callback 与 App frame 并发产生事件
- **THEN** callback 在有界时间内返回，事件只在后续 system ingestion 阶段影响快照

### Requirement: 容量与拒绝可诊断
平台事件、App command 和 service 内部队列 SHALL 使用明确固定容量，不得逐帧堆分配；满队列、无效命令、不可用服务和超出 frame budget SHALL 返回或记录稳定的拒绝原因、累计计数和 high-water mark。

#### Scenario: 命令队列已满
- **WHEN** App 在一个 frame 内提交超过容量的命令
- **THEN** 超出容量的命令被明确拒绝，已接受命令保持 FIFO，系统继续 update/render 且诊断计数增加

#### Scenario: 服务不可用
- **WHEN** App 向 unavailable 的 voice service 提交启动命令
- **THEN** 命令不会伪造 running 状态，并产生可通过快照或诊断观测的拒绝结果

### Requirement: 系统服务使用注入时间并可无设备运行
系统服务 SHALL 使用注入的 monotonic/simulation time 和 platform ports，并 SHALL 能在 no-op 或 headless adapter 下确定性运行；core/system 源码不得包含 Arduino、ESP-IDF、SDL 或板级头文件。

#### Scenario: Headless replay
- **WHEN** 测试以固定时间和平台事件序列推进 system host
- **THEN** 每帧快照、命令结果和诊断序列可重复且无需连接设备
