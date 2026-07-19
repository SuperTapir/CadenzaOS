## Context

现有运行时已经把 Cover 定义为 const、静态、无分配、双 profile 等尺寸 blit，并在 launch p0 与 return bridge 复用同一像素。问题在母图：MOTION 同时使用速度线、箭头、靶心和底轨；SETTINGS 平铺带刻度与螺丝的三件硬件；GALLERY 使用帧号、粒子、方块和多段轨迹。它们解释了功能，却没有 TIMER 那种只靠标题与一个功能关系就能记住的身份。

## Goals / Non-Goals

**Goals:**

- 三张图分别拥有可一句话描述、在硬阈值下仍成立的功能钩子。
- 共享大标题、粗轮廓、强黑白块面、少量结构灰阶与安全边距，但不共享同一构图模板。
- 用户批准后从同一 lossless 母图直接生成 350×155/280×124 PBM、预览和 packed headers，并走完整门禁。

**Non-Goals:**

- 不改 TIMER、App UI、launch renderer、Launcher chrome、Cover API 或静态像素契约。
- 不新增 Cover 状态变体，不把测试/hash 当作视觉批准。

## Decisions

### 1. 三个视觉 dialect 只共享媒介纪律，不共享标题位置

- MOTION：明亮 typographic signal；一个大空心环沿单轨道牵引被惯性拉斜的 `MOTION`。
- SETTINGS：明亮 retro software package；`SETTINGS` 成为一块共同基座上的结构、铭牌或跨幅 lockup，旋钮、拨杆、滑块与标题形成一个控制台整体，而不是再次采用左字右物。
- ANIMATION GALLERY：暗色 print-zine；两行标题可与同一 figure 的恰好三个姿态和一条 easing/spring 路径上下穿插、跨场或形成舞台关系，不预设标题位于左栏。

每张只允许标题、一个主 motif、一个宽支撑关系和至多两个语义 accent。标题位置、尺度关系和阅读方向 SHALL 针对 App 功能重新构图；`MOTION` 的左低字标只属于该张，不成为系列模板。灰只解释运动、重叠、正面/侧面或舞台深度；纯黑白不成立的候选直接重做。

### 2. 一张一张生成与审批

候选写入日期化 review 目录，保留生成原图、明确 350:155 crop、1400×620 master、350×155/280×124 grayscale、pure 与 reflective preview、prompt 和审阅记录。每张取得用户明确批准后才运行正式 ordered-dither/pack 管线并开始下一张；未批准候选不得覆盖 source/PBM/header/golden。

### 3. 正式集成仅替换资产派生链

批准母图覆盖对应 `source/<app>.png`，直接生成两个 canonical PBM，再由 `tools/pack_bitmap.py` 刷新同 symbol header。若需要运行时代码修图或分支，则退回母图迭代。README 记录参数、provenance 与批准日期。

### 4. Golden 更新晚于审美批准

先检查标题/motif、硬阈值、1× 轮廓、open counters 和一像素移动的 dither 稳定性，再更新 snapshots。正式转换显式启用 `--edge-cleanup`：只把同时邻接近黑/近白的抗锯齿过渡像素及其 1 px guard band 改为 coverage threshold，宽阔结构灰面仍走 ordered dither。每张另做 nearest-neighbor 4× 检查，拒绝贴着标题、baseline、counter 或主 silhouette 的 Bayer 点、梳齿、针孔和灰色毛边；规则像素阶梯不算缺陷。资产来源、PBM/header、immutability、handoff、完整 host/desktop/headless/firmware-compatible build 与 size gate 证明工程集成；真实硬件仍是 Memory LCD 闪烁最终验收。

### 5. 母图必须先像插画，再谈 1-bit 转换

SETTINGS 等浅色 Cover SHALL 以丝网印刷、纸雕或技术海报式的离散块面构造体积：主外轮廓必须是一条连续、足够粗的实色边界，内部只使用三至五个边界明确的平面。候选若依赖棚拍式柔光、连续渐变、环境遮蔽、镜面高光、圆润倒角或接触阴影来成立，即使构图和文字正确，也必须在转换前退回重做；`--edge-cleanup` 只负责清理缩放抗锯齿，不能把照片式渲染补救成插画。

### 6. Cover pipeline 采用可回退的分阶段门禁

资产按 `brief → master candidate → pixel candidate → user-approved baseline → integrated → verified` 推进。主观门禁负责功能钩子、构图、插画感和精确文字；确定性门禁负责硬阈值、双 profile、ordered dither、edge cleanup、4× 像素检查、pack/check、snapshots 与固件体积。`prepare_cover.py` SHALL 自动产出双 profile 的 texture-independent hard-threshold preview、ordered-dither pure/reflective preview 与 nearest-neighbor 4× preview，避免规范要求依赖临时手工命令。任何用户反馈或验证失败都回到其所属阶段，不允许用后续工具掩盖上游失败。

## Risks / Trade-offs

- [生成器拼错文字或加装饰] → 锁定逐字标题与允许元素，失败只做单点迭代。
- [三张仍显同质] → 只共享轮廓与灰阶纪律，分别采用牵引字标、共同控制台、三姿态角色。
- [简化后功能过抽象] → motif 必须对应真实 App 行为，若可换给其他 App 同样成立则拒绝。
- [dither 随轨道移动闪烁] → 只用宽阔低频灰面，检查 1 px track，必要时减少 tonal bands。
- [抗锯齿灰像素被误当作材质抖动而形成毛刺] → edge-cleanup 固化黑白轮廓及 1 px guard，并以合成边界/灰面回归测试与 4× A/B 证明只清轮廓、不抹灰面。
- [棚拍/产品渲染在 1-bit 后变成照片转网点] → 在任何 PBM 审批前先检查灰阶母图；连续光照、软阴影、镜面高光和依赖 dither 才能看清的外轮廓均直接拒绝，改用硬边离散插画平面。
- [golden 刷新掩盖错误] → 只有已批准母图的可复现派生结果可以更新预期值。

## Migration Plan

逐张生成/审批 → 生成双 profile 资产与 headers → 更新文档/goldens → 全量验证 → 审计 TIMER hash 与 diff → commit。回滚只恢复三组 source/PBM/header/golden，不涉及 API 或数据迁移。

## Open Questions

- 三张具体像素基线均已由用户逐张明确批准。
- Memory LCD 实机 dither 拖影需提交后真机观察，host 预览不能替代。
