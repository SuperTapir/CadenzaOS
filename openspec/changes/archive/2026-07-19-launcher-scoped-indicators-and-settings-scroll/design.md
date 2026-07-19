## Context

Cadenza 的被动系统提示目前有两处硬编码合成点：

1. `AppRuntime::renderWithSystem`：后台 Timer Running/Paused 且 `owner != currentId` 时画左上角 `statusIndicator`
2. `FrameCoordinator`：`voice.microphoneInUse` 时在右上角画 `MIC`

两者都会盖在任意前台 App 上。产品意图改为：Launcher 充当系统仪表盘，业务 App 保持干净画面。同时 Settings 主列表硬编码 7 行整表排布、无 `scrollOffset`，在 320×170 上末项常落在可视区外。

约束：保持固定合成层级、无逐帧分配、不引入通用 overlay API；Timer 服务与 TimerAlert critical 行为不变；USB mic 的声音抑制策略不变。

## Goals / Non-Goals

**Goals:**

- 用显式 **indicator scope** 表达被动角标的可见面，首批 Timer 后台角标与 MIC 角标均为 **HomeOnly**
- Settings 主列表在 overflow 时滚动，选中行始终完整可见，旋钮可到达最后一项
- 用自动化锁定「非 Home 不可见 / Home 可见」与「末项可达」

**Non-Goals:**

- 不改变 TimerAlert、System Menu、transient toast 的优先级与输入所有权
- 不把 MIC 隐私策略做成用户可配置项（本变更只改呈现 scope）
- 不引入 App 可注册的自定义 indicator / overlay contribution
- 不做通用虚拟列表框架；Settings 只需选中跟随滚动
- 不改变 USB mic 期间的 cue 抑制逻辑

## Decisions

### 1. IndicatorScope 枚举，而不是散落 `if (isHome)`

采用封闭枚举（名称可微调，语义固定）：

| Scope | 含义 | 首批消费者 |
|-------|------|------------|
| `HomeOnly` | 仅当 `currentId == homeId` 时绘制 | Timer 后台角标、MIC |
| `Global` | 任意画面（含 Menu 之上的 persistent 层，若未来需要） | 保留扩展位，本变更不接消费者 |
| `NonOwner` | 历史 Timer 行为；本变更不再作为默认 | 可不实现调用点，或保留枚举供回归对照 |

**为何：** 用户明确希望「类似东西」能选择只盖在 Launcher；枚举比两处布尔更可演进，且符合现有「不把 Toast/Indicator/Menu 糊成一个 `showOverlay(bool)`」的决策。

**备选否决：** 仅改两处 `if (currentId == homeId)`——能修眼前问题，但下次第三个角标仍会分叉。

### 2. Home 判定用 Runtime 的 `homeId`，不是 App 名字字符串

可见性判定：`runtime.currentId() == runtime.homeId()`（或等价已有 API）。Launcher 即 Home composition root。

System Menu / TimerAlert 打开时 current AppId 不变：若底层仍是非 Home App，HomeOnly 角标不显示——符合「别挡业务画面」；回到 Launcher 后再显示。

### 3. Timer 后台角标与 MIC 均 HomeOnly

产品选择：两者都只在 Launcher。MIC 在非 Home 前台不可见是有意的隐私提示弱化，换取画面干净；服务层 `microphoneInUse` / cue 抑制仍生效。

TimerAlert（到期 critical）仍全局覆盖，与角标 scope 无关。

### 4. 合成点集中判定

推荐：抽一个小的 pure helper / coordinator 方法（例如 `shouldShowPassiveIndicator(scope, currentId, homeId)`），Timer 与 MIC 绘制前共用。避免 `AppRuntime` 与 `FrameCoordinator` 各写一套语义。

MIC 可继续留在 `FrameCoordinator` 绘制，但 scope 门闩必须与 Timer 一致。

### 5. Settings：选中跟随滚动，不改交互模型

- 保持 `wrap(selected, itemCount)` 旋钮循环
- 增加 `scrollOffset`（行索引或像素，优先行索引以保持确定性）
- 可视窗口由 header/footer 与 canvas 高度推导；选中变化时把 offset clamp 到使选中行完全落在窗口内
- 不引入惯性、滚动条控件或翻页模式

**备选否决：** 缩小字号硬塞 7 行——与已采纳的舒适 typography 冲突。

### 6. 测试迁移策略

- 删除/改写「indicator over every built-in App」：改为 Home 有像素、非 Home（至少 Settings/Motion）无角标像素
- MIC 测试在非 Home fixture 上改为断言不可见；另加 Home + streaming 可见
- Settings：旋到最后一项时选中行 bounds 在 canvas 内；双 profile 覆盖

## Risks / Trade-offs

- [Risk] MIC 在业务 App 中不可见，用户可能不知道麦仍开着 → Mitigation：设计记录为有意取舍；cue 抑制仍在；若真机反馈差，可把 MIC 单独升回 `Global` 而不改 Timer
- [Risk] System Menu 盖在非 Home App 上时看不到 Timer 角标 → Mitigation：接受；到期仍有 TimerAlert；回 Launcher 即恢复角标
- [Risk] Settings snapshot 基线漂移 → Mitigation：更新双 profile Settings / background-timer 相关 snapshot，并保留 failure PNG 审计
- [Risk] scope 枚举过度设计 → Mitigation：首版只接 `HomeOnly` 调用点，`Global` 可留枚举不接线

## Migration Plan

1. 先改失败测试（Home 可见 / 非 Home 不可见 / Settings 末项可见）
2. 落地 scope 判定与两处绘制门闩
3. 落地 Settings scrollOffset
4. 更新 snapshot 基线与相关 E2E
5. 回滚：恢复绘制条件与去掉 scroll 即可；无持久化格式变更

## Open Questions

无阻塞项。若实现中发现 Home 在 Menu suspend 期间需要例外显示角标，再单开 delta；当前默认不显示。
