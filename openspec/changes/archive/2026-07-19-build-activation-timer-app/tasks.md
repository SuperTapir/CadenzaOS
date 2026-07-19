## 1. 调研与基线

- [x] 1.1 保存 Timer App、System Menu、声音 PCM、双 profile snapshot、desktop smoke 与 firmware size 的变更前基线
- [x] 1.2 完成并审计 AOSP、Zephyr、FreeRTOS、ESP-IDF 与同类产品的版本、许可证、关键源码和采用/不采用结论
- [x] 1.3 添加 source/license/platform boundary audit，证明 Timer 不引入第三方源码、RTOS callback 或 App 平台时钟依赖

## 2. Timer 领域契约与失败测试

- [x] 2.1 先写 TimerService Ready/Running/Paused/Expired、1/60 分钟边界和 inclusive deadline 失败测试
- [x] 2.2 先写 timestamp regression、large step、single expiration generation、no-backlog 与 deterministic replay 失败测试
- [x] 2.3 先写 TimerControl capability、owner、非法状态/时长、queue/commit 与 zero-allocation 失败测试
- [x] 2.4 定义 TimerState、TimerSnapshot、typed commands、AppCapability 和稳定 diagnostics/rejection
- [x] 2.5 实现 fixed-state TimerService、monotonic deadline、pause/resume/setRemaining/acknowledge 与 snapshot

## 3. 时间注入与 SystemServiceHost 集成

- [x] 3.1 先写 presentation delta 为零/被 clamp 而 monotonic time 继续的失败 contract test
- [x] 3.2 实现 `beginFrameAt`/`runFrameAt` 显式 monotonic 路径并保留确定性 fallback time API
- [x] 3.3 将 TimerService 接入 begin/update snapshot、FIFO command commit、expiration edge 和 owner session lifetime
- [x] 3.4 在 firmware 使用 64-bit ESP monotonic time、desktop 使用 SDL ticks、headless 使用 injected simulation time
- [x] 3.5 更新 frame order、platform-header、shared-source 和 direct API tests

## 4. Timer App

- [x] 4.1 先写 Ready turn/start、Running pause/boundary、Paused adjust/resume、Expired no-App-input 和离开/返回状态测试
- [x] 4.2 将 TimerApp 从 elapsed demo 迁移为只读 TimerSnapshot + typed command 状态机，保持 catalog id 和获批 Cover
- [x] 4.3 实现 320×170 与 400×240 的大号 MM:SS、time mass、分钟刻度、状态/操作提示和 Reduced Motion 表达
- [x] 4.4 补齐真实状态反馈音、单帧 turn 聚合、边界和失败视觉反馈
- [x] 4.5 更新 Timer App unit tests、allocation test 与双 profile deterministic snapshots

## 5. 后台 Indicator 与 Critical Alert

- [x] 5.1 先写 owner App 外 indicator、Menu active expiry、其他 App expiry、transition expiry 和 current App lifecycle 失败测试
- [x] 5.2 先写 held-button-at-expiry、release arming、确认 sequence、Menu hidden action 不执行和恢复失败 trace
- [x] 5.3 扩展 SystemSurfaceCoordinator 的 TimerAlert critical priority、frozen frame、sequence ownership、ack intent 与 diagnostics
- [x] 5.4 实现 Timer 后台 persistent indicator、Timer owner 前台抑制和双 profile critical alert renderer
- [x] 5.5 更新 composition order、surface size/allocation、snapshot/golden 和 system overlay 回归测试

## 6. TimerComplete 声音

- [x] 6.1 先写 TimerComplete definition/profile/event structure、deadline edge、repeat no-backlog、Muted 和 acknowledge stop 失败测试
- [x] 6.2 实现 TimerComplete 参数化 cue、Expired edge/有界 repeat cadence 与确认 stopAll 语义
- [x] 6.3 生成并审计 TimerComplete PCM golden、peak/RMS/event offset/回零报告和上下文试听 WAV
- [x] 6.4 更新 interaction sound/source audit、sound cue dump、15+1 cue 回归与音频并发测试

## 7. E2E、文档与完成门禁

- [x] 7.1 实现 Launcher→Timer→Start→System Menu/Home→Expire→Acknowledge→Timer→Start 的 headless deterministic E2E
- [x] 7.2 更新 desktop smoke/CLI 操作说明，并验证 held button、transition 和 Muted 到期真实流程
- [x] 7.3 更新平台架构、Timer 实现报告、验证说明、snapshot/PCM baseline 和实体 T-Embed 验收清单
- [x] 7.4 运行相关测试、完整 host、strict warnings、sanitizer、source/allocation audit、SDL build 与 desktop smoke
- [x] 7.5 运行 PlatformIO T-Embed 与候选共享源码构建、size report、OpenSpec validate 和 `git diff --check`
- [x] 7.6 按 proposal/spec/tasks 逐项完成审计，记录未在实体设备执行的旋钮手感、响度与长时间运行范围
