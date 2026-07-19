# Launcher Covers

`timer.pbm`、`motion.pbm`、`settings.pbm` 与 `gallery.pbm` 是四个内置
App 经人工确认后的 350×155、1-bit 封面。`*_t_embed.pbm` 是直接从同一
高分辨率母图生成的 280×124 T-Embed 版本。它们以屏幕像素为
最终审阅单位，不在设备运行时解码 PNG、缩放或分配临时 framebuffer。

`source/` 保存四张清理后的原始 PNG，用于未来重新调阈值、支持新尺寸或
重新导出；固件不直接编译这些 PNG。

源插图由用户提供并经 AI 辅助精简；使用 Pillow 的 LANCZOS 缩放到
350×155 与 280×124。新版 MOTION、SETTINGS 与 GALLERY 使用五档 ordered dither，并启用通用
`--edge-cleanup`：同时邻接近黑与近白的抗锯齿过渡像素及其 1 px guard band
改用 coverage threshold，宽阔灰面仍保留 ordered dither，避免标题、圆环和
baseline、控制台或人物轮廓的灰边变成 Bayer 毛刺。SETTINGS 的母图另经插画门禁，
拒绝连续产品渲染光照，要求硬边离散平面与独立的实色外轮廓。TIMER 使用五档 ordered dither，并在已批准标题区域保留原 coverage hint，避免
改变其既有 hash。
禁止从已二值化的 PBM 二次缩放，否则细线会碎裂并产生孤立黑点。

MOTION 的可复现生成与检查命令：

```sh
python3 .codex/skills/generate-launcher-cover-art/scripts/prepare_cover.py \
  assets/launcher-covers/source/motion.png assets/launcher-covers \
  --name motion --levels 5 --edge-cleanup

python3 .codex/skills/generate-launcher-cover-art/scripts/prepare_cover.py \
  assets/launcher-covers/source/motion.png assets/launcher-covers \
  --name motion --levels 5 --edge-cleanup --check
```

GALLERY 使用相同管线：

```sh
python3 .codex/skills/generate-launcher-cover-art/scripts/prepare_cover.py \
  assets/launcher-covers/source/gallery.png assets/launcher-covers \
  --name gallery --levels 5 --edge-cleanup --check
```

SETTINGS 使用相同管线：

```sh
python3 .codex/skills/generate-launcher-cover-art/scripts/prepare_cover.py \
  assets/launcher-covers/source/settings.png assets/launcher-covers \
  --name settings --levels 5 --edge-cleanup --check
```

非 `--check` 模式还会自动生成双 profile 的硬阈值、ordered-dither pure/reflective
及 nearest-neighbor 4× 审阅图；硬阈值负责证明标题与主轮廓不依赖灰阶，4× 图负责
区分规则像素阶梯与贴边 Bayer 毛刺。

这些工具需要 Pillow；CMake 使用的 Python 能导入 Pillow 时会自动注册八个母图
来源检查，没有 Pillow 时仍保留不依赖 Pillow 的八个 PBM→header 检查。

TIMER 的可复现生成与检查命令：

```sh
python3 .codex/skills/generate-launcher-cover-art/scripts/prepare_cover.py \
  assets/launcher-covers/source/timer.png assets/launcher-covers \
  --name timer --levels 5 \
  --solid-light-region 0.026 0.40 0.70 0.92

python3 .codex/skills/generate-launcher-cover-art/scripts/prepare_cover.py \
  assets/launcher-covers/source/timer.png assets/launcher-covers \
  --name timer --levels 5 \
  --solid-light-region 0.026 0.40 0.70 0.92 --check
```

生成 packed headers：

```sh
python3 tools/pack_bitmap.py assets/launcher-covers/timer.pbm \
  lib/cadenza_apps/src/generated/timer_cover.h \
  --symbol kTimerCover \
  --provenance "Cadenza original AI-assisted 3D TIMER artwork, user-approved 2026-07-19; canonical PBM in assets/launcher-covers/timer.pbm"
```

`motion`、`settings`、`gallery` 以及对应的 `*_t_embed` 使用同样命令与
对应符号。CMake host tests 会使用 `--check` 验证八个 header 与 PBM
逐字节一致。
