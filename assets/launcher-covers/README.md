# Launcher Covers

`clock.pbm`、`motion.pbm`、`settings.pbm` 与 `gallery.pbm` 是四个内置
App 经人工确认后的 350×155、1-bit 封面。`*_t_embed.pbm` 是直接从同一
高分辨率母图生成的 280×124 T-Embed 版本。它们以屏幕像素为
最终审阅单位，不在设备运行时解码 PNG、缩放或分配临时 framebuffer。

`source/` 保存四张清理后的原始 PNG，用于未来重新调阈值、支持新尺寸或
重新导出；固件不直接编译这些 PNG。

源插图由用户提供并经 AI 辅助精简；使用 Pillow 12.2.0 的 LANCZOS `fit`
分别直接裁切到 350×155 与 280×124，再按各图固定灰度阈值转为 PBM。
禁止从已二值化的 PBM 二次缩放，否则细线会碎裂并产生孤立黑点。

生成或检查目标 PBM（以 Motion T-Embed 为例）：

```sh
python3 tools/convert_launcher_cover.py \
  assets/launcher-covers/source/motion.png \
  assets/launcher-covers/motion_t_embed.pbm \
  --width 280 --height 124 --threshold 170

python3 tools/convert_launcher_cover.py \
  assets/launcher-covers/source/motion.png \
  assets/launcher-covers/motion_t_embed.pbm \
  --width 280 --height 124 --threshold 170 --check
```

该工具需要 Pillow；CMake 使用的 Python 能导入 Pillow 时会自动注册八个母图
来源检查，没有 Pillow 时仍保留不依赖 Pillow 的八个 PBM→header 检查。

生成 packed headers：

```sh
python3 tools/pack_bitmap.py assets/launcher-covers/clock.pbm \
  lib/cadenza_apps/src/generated/clock_cover.h \
  --symbol kClockCover \
  --provenance "Cadenza original AI-assisted artwork, user-approved 2026-07-18; canonical PBM in assets/launcher-covers/clock.pbm"
```

`motion`、`settings`、`gallery` 以及对应的 `*_t_embed` 使用同样命令与
对应符号。CMake host tests 会使用 `--check` 验证八个 header 与 PBM
逐字节一致。
