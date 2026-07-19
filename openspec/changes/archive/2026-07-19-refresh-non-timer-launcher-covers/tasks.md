## 1. 视觉基线

- [x] 1.1 记录三张旧 Cover 的说明图/高频噪声失败用例并锁定 TIMER hash
- [x] 1.2 为三张新版分别确定 identity brief、元素预算、tonal plan 与视觉 dialect

## 2. 候选与审批

- [x] 2.1 生成并审阅 MOTION lossless、硬阈值及双 profile 候选
- [x] 2.2 取得 MOTION 用户明确视觉批准
- [x] 2.3 以失败测试实现 contour edge-cleanup、更新 Cover skill，并重新导出/4×审阅 MOTION
- [x] 2.4 生成并审阅 SETTINGS v1；记录其在正式 reflective preview 中暴露的弱外轮廓与照片转网点问题，并撤销该视觉基线
- [x] 2.5 以硬边离散平面重新生成、审阅并取得 SETTINGS v2 用户明确批准
- [x] 2.6 生成、审阅并取得 ANIMATION GALLERY 用户明确批准
- [x] 2.7 将 Cover skill 重构为 master/pixel/approval/integration 分阶段门禁，并让脚本自动生成硬阈值与 4× 审阅产物

## 3. 资产集成

- [x] 3.1 从批准母图生成六张 canonical PBM 与 pure/reflective previews
- [x] 3.2 替换三张 source PNG、六张 PBM 并证明 TIMER 三项 hash 不变
- [x] 3.3 重新打包六个 headers 并更新 provenance、参数和 README

## 4. 验证

- [x] 4.1 运行来源复现、PBM/header、尺寸/色阶与孤立像素资产检查
- [x] 4.2 更新并审阅双 profile Launcher、launch p0、return bridge 与静态交互 snapshots
- [x] 4.3 运行 Cover immutability、handoff、30 FPS 像素变化与 moving-track dither 检查
- [x] 4.4 运行完整 host、desktop/headless E2E、firmware-compatible build 与 size gate

## 5. 交付

- [x] 5.1 审计 diff、来源、TIMER 不变证据并运行 `git diff --check`
- [x] 5.2 提交 OpenSpec、正式资产、headers、goldens 与文档
