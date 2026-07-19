# MOTION v1 prompt record

## Identity brief

MOTION 是 spring/inertia 实验；一个很大的空心质量环沿单条轨道牵引被惯性拉斜的 `MOTION` 字标，标题本身承担运动感，整体以明亮白底区别于 TIMER 的暗色立体遮挡。

## Permitted elements

1. 精确标题 `MOTION`，只出现一次。
2. 一个内部保持纯白的大空心圆环。
3. 一条简单水平轨道。
4. 一个目标标记。
5. 仅在必要时使用至多三条宽速度痕。

不得发明其他物件；不得使用网格、粒子、箭头串、测量刻度、靶心、第二件仪器或底部机械轨。

## Tonal plan

- 主白块：安静背景与圆环内部。
- 主黑块：完整字标、圆环外缘与单轨道。
- 浅灰面：圆环后方的一块运动滞后面，只解释惯性。
- 深灰面：圆环与字标交叠处的分离面，只解释遮挡。

## Generation prompt

Built-in image generation used `assets/launcher-covers/source/timer.png` only as a reference for safe margins, confident contour weight, sparse industrial grayscale massing, and finish. The prompt explicitly prohibited copying TIMER's layout, tilted dial, foreground-letter overlap, object, typography placement, or dark composition. It requested an exact 350:155 crop-ready monochrome master with the permitted elements and tonal plan above, smooth antialiased contours, hard-threshold recognition, no external frame, no Launcher chrome, no unrelated Playdate or third-party trade dress, and exact spelling `M-O-T-I-O-N`.

Generated with the built-in `image_gen` path. Original output is retained as `source/motion-v1-original.png`; the explicit 350:155 center crop is retained separately from the 1400×620 review master.
