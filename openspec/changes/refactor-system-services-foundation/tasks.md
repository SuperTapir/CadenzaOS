## 1. 基线、调研与决策门禁

- [x] 1.1 保存 host/desktop/firmware、snapshot、PCM hash、RAM/flash 与 source-boundary 现状基线，并写出可重复命令
- [x] 1.2 建立系统服务调研记录，锁定 Zephyr zbus、Matter、Pigweed 与 ETL 的 commit/license/关键源码，记录采用、拒绝和已知失败语义
- [x] 1.3 建立 voice/USB/board 调研记录，锁定 TinyUSB、ESP IoT Solution、Arduino-ESP32、ESP-IDF 与 T-Embed 的 commit/license/关键源码和待真机假设
- [x] 1.4 建立隔离的 ESP-IDF 5.x + Arduino component + TinyUSB UAC2/CDC build spike，记录依赖锁、sdkconfig、复现命令、descriptor endpoint 与 size/task/stack 结果
- [x] 1.5 形成 SDK 采用/不采用决策；若迁移门槛未满足，保留可审计 blocker 且不修改本机 Arduino 2.0.17 framework package

## 2. App 能力边界与模块拆分

- [x] 2.1 先写 failing tests，覆盖 core-unknown AppId、invalid/duplicate/capacity 注册和非默认 Home App 导航
- [x] 2.2 将 `AppId` 改为 core value type、实现 fixed-capacity App catalog，并把 built-in id/constants 移入 Apps 层
- [x] 2.3 先写 failing compile/runtime tests，证明 App update/render 只获得窄 context 且不能访问完整 Runtime/service
- [x] 2.4 实现 update/render contexts、AppNavigator 与 catalog view，迁移全部 bundled Apps 并保持 lifecycle/transition/input 行为
- [x] 2.5 创建独立 `cadenza_apps` target，将 Apps/Gallery 从 `cadenza_core` 移出，并让 headless/desktop/firmware 组合根显式装配同一 catalog
- [x] 2.6 运行 lifecycle、snapshot、source-manifest 与 core-alone link 回归，确认两种 display profile 视觉 hash 不变

## 3. SystemServiceHost 与现有服务迁移

- [x] 3.1 先写 failing frame-trace tests，覆盖平台事件摄取、冻结 update snapshot、FIFO commit、同帧 render snapshot 和 callback 非重入
- [x] 3.2 实现固定容量 typed platform events、system commands、diagnostics、`SystemSnapshot` 与单一 frame coordinator
- [x] 3.3 先写 failing capacity tests，覆盖 event/command queue 满、无效/不可用命令、per-frame budget、high-water 和稳定拒绝原因
- [x] 3.4 将 sound/volume/settings authoritative state 迁入 system services，以显式 output port 供 SDL/headless/T-Embed 消费
- [x] 3.5 将 motion 状态迁入 system service/snapshot，并移除 App 对 Runtime service accessor 的剩余依赖
- [x] 3.6 实现 USB voice active 时 cue 默认抑制和视觉等价反馈，保持现有 SoundCue PCM/hash 在非抑制路径不变
- [x] 3.7 更新三类组合根和生命周期顺序，运行 system、App、sound、snapshot、desktop 与 current firmware 回归

## 4. Portable Voice Input Core

- [x] 4.1 先写 failing PCM contract tests，覆盖 48 kHz S16 mono、480-sample block、sample 顺序与格式拒绝
- [x] 4.2 实现固定格式 voice block、capture states、consumer intents、diagnostics 与单一 capture coordinator
- [x] 4.3 先写 failing fan-out tests，覆盖每 consumer 固定 SPSC queue、慢 consumer 独立 drop-newest/overrun、无阻塞与恢复
- [x] 4.4 实现 analyzer 与 USB 独立 consumer fan-out，不逐 block 分配、不向普通 App 暴露 raw PCM
- [x] 4.5 先写 failing analyzer tests，覆盖 RMS/peak、VAD attack/release、silence、clipping、error 和 deterministic replay
- [x] 4.6 实现 App voice start/stop commands、voice snapshot 与持续视觉隐私状态
- [x] 4.7 实现可注入静音/波形/语音 burst/error/overrun 的 headless microphone adapter 和确定性 fixtures
- [x] 4.8 运行 strict warning、sanitizer、burst/backpressure 与长时间模拟时钟 tests，记录容量 high-water 和 host memory 结果

## 5. USB Voice Device 与平台适配

- [x] 5.1 先写 failing USB tests，覆盖 UAC2 mono descriptor、CDC composite、interface/endpoint 唯一性、硬件 endpoint 上限和 MIC-only 配置
- [x] 5.2 实现 portable USB packetizer，将 480-sample blocks 切为 1 ms/48-sample packets，并覆盖 partial underflow silence/counter
- [x] 5.3 先写 failing stream lifecycle tests，覆盖 alt-setting inactive、disconnect、stale flush、reactivation silence 和 shutdown ownership
- [x] 5.4 在已通过门禁的 SDK baseline 上实现 UAC2+CDC adapter，连接独立 USB voice consumer，保留 build-time hardware feature 边界
- [x] 5.5 实现 ES7210/I²S1 adapter 的板级配置、DMA chunk normalization、S16 saturation、partial block、timeout/error 和有界 retry
- [x] 5.6 明确 I²S0 speaker 与 I²S1 microphone task/DMA/buffer ownership，加入输入输出并发 build/host model tests
- [x] 5.7 输出 candidate firmware 依赖、descriptor、RAM/flash/task/stack 报告并与迁移前基线比较

## 6. Wi-Fi/BLE 后续能力的公共契约预留

- [x] 6.1 用最小 compile/test fixture 验证连接 adapter callback 只能进入有界平台事件队列，App 只见 typed command/snapshot
- [x] 6.2 记录 ESP32-S3 仅 BLE、无 Classic HFP/A2DP，以及普通 BLE/Wi-Fi 不构成 macOS 系统麦克风的产品边界
- [x] 6.3 记录后续 provisioning、credential storage、RF coexistence、功耗与 App network API 所需的独立 OpenSpec，不在本 change 提前暴露原始 socket/GATT callback

## 7. 自动化完成与文档

- [x] 7.1 更新架构、模块依赖、frame transaction、App capability、voice session、privacy、backpressure 和 composition root 文档
- [x] 7.2 更新构建/开发文档，包含 current 与 candidate firmware 命令、依赖锁、license/attribution、失败恢复和不允许的本机 framework patch
- [x] 7.3 运行完整 host、strict-warning、sanitizer、snapshot、SDL dummy/E2E、core-alone/source-boundary、current firmware、candidate UAC firmware 和 OpenSpec strict 门禁
- [x] 7.4 保存自动化证据和未执行实体证据清单，明确声明 `host foundation complete / hardware validation pending`

## 8. 实体 T-Embed 与 Mac 验收（设备到货后）

- [ ] 8.1 验证 ES7210 双麦通道、增益、噪声、clipping、48 kHz 时钟、延迟、I²S timeout/retry 和持续隐私指示
- [ ] 8.2 验证 macOS System Information/Audio MIDI Setup 枚举、系统输入选择、格式与常用录音 App 采音
- [ ] 8.3 执行 disconnect/reconnect、sleep/wake、alt-setting、至少 30 分钟录音和 USB overrun/underflow/时钟漂移测试
- [ ] 8.4 执行前台 App analyzer + USB 并发、系统 cue 抑制/回录、I²S 输入输出并发和错误降级测试
- [ ] 8.5 在 Wi-Fi/BLE 启用条件下测量 RF coexistence、RAM/task/stack、功耗与语音稳定性，并记录采用限制
- [ ] 8.6 所有实体门禁通过后更新证据、关闭 hardware pending 并准备同步/归档 OpenSpec change
