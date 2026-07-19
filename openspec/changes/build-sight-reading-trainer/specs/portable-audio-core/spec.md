## MODIFIED Requirements

### Requirement: 固定 SPSC 语义命令队列
系统 SHALL 提供固定 16 项的单生产者/单消费者命令队列，承载 PlayCue、PlayNotes、SetVolume 与 StopAll；消费者 SHALL 是 voice 状态的唯一所有者，队列 SHALL 为关键命令保留至少 4 项容量且生产者不得覆盖消费者可见 slot。Navigate、Boundary 与 PlayNotes SHALL 使用普通可丢弃水位。Muted 与 StopAll SHALL 另有无锁安全邮箱，使队列饱和时仍能在下一次 render 前清空声音。

#### Scenario: 正常生产消费
- **WHEN** 主线程按顺序提交多个 cue、note set 与控制命令且音频消费者排空队列
- **THEN** 命令以提交顺序生效且不需要锁或动态分配

#### Scenario: 重复内容溢出
- **WHEN** 普通命令已达到非关键水位且新 Navigate、Boundary 或 PlayNotes 到达
- **THEN** 请求被拒绝并记录 overflow，保留容量不被占用且历史请求不会稍后补播

#### Scenario: 关键提示使用保留容量
- **WHEN** 普通命令已达到非关键水位且 Confirm、Back、Reject、SetVolume 或 StopAll 到达
- **THEN** 关键命令在保留容量内被接受且全部既有命令顺序不变

#### Scenario: 饱和队列不能阻塞静音
- **WHEN** 16 项队列已被关键 cue 填满且主线程请求 Muted
- **THEN** 请求仍被接受，消费者清空既有与排队 voice，并在该 render 输出精确零

### Requirement: 四声部与平滑确定性抢占
系统 SHALL 提供固定四声部和固定容量 delayed event；满载时 SHALL 仅在新 tone priority 不低于现有最低 priority 时选择其中最老的 voice，并 SHALL 先短 release 再启动 pending tone。有效 MusicalNoteSet SHALL 同时使用不超过四声且重播前 SHALL 清理旧 audition state。

#### Scenario: 延迟 event 到点
- **WHEN** cue 包含尚未到 offset 的 tone
- **THEN** consumer 在目标样本处提交 tone，且任何时刻同时 active 的 palette voice 不超过四声

#### Scenario: 高优先级抢占
- **WHEN** 四个 voice 已满且触发更高优先级 tone
- **THEN** 最低优先级中最老的 voice 平滑释放，随后渲染新 tone

#### Scenario: StopAll 清理延迟 event
- **WHEN** cue 或 MusicalNoteSet 有 active、pending 或尚未起音的 delayed state 且消费 StopAll 或 Muted
- **THEN** active、pending 与 delayed 状态全部清空，历史 event 不会稍后补播

#### Scenario: 三音 MusicalNoteSet
- **WHEN** consumer 接受三个合法 MIDI note
- **THEN** 三个 tone 同时起音、active voice 不超过四且固定容量不分配 heap

## REMOVED Requirements

### Requirement: 三声部与平滑确定性抢占
**Reason**: 当前实现与已归档声音 palette 已经使用固定四声部，原三声部 requirement 与同一主 spec 中的四声部 requirement 冲突；SIGHT 三和弦也以现有四声部为容量边界。

**Migration**: 使用本 delta 中完整的“`四声部与平滑确定性抢占`”requirement；抢占优先级、最老 voice、短 release 与无 heap 语义保持不变。
