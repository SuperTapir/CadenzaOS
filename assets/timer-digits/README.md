# Timer numeral atlases

`tools/generate_timer_digits.py` 生成 Cadenza 自有的工业仪表数字 atlas。运行时只读取已打包的 1-bit header，不依赖字体引擎、Pillow 或 heap。

- T-Embed：digit cell `50×84`、colon cell `28×84`，组合 `MM:SS` 为 `228×84`。
- Sharp：digit cell `68×120`、colon cell `36×120`，组合 `MM:SS` 为 `308×120`。
- `source/timer_digits_master.svg` 是平滑可编辑母稿；两个 PBM 是原生 profile 资产。

生成命令：

```bash
python3 tools/generate_timer_digits.py
python3 tools/pack_bitmap.py assets/timer-digits/timer_digits_t_embed.pbm \
  lib/cadenza_core/include/cadenza/presentation/generated/timer_digits_t_embed.h \
  --symbol kTimerDigitsTEmbed \
  --provenance "Cadenza-authored industrial Timer numerals; native 528x84 atlas from tools/generate_timer_digits.py"
python3 tools/pack_bitmap.py assets/timer-digits/timer_digits_sharp.pbm \
  lib/cadenza_core/include/cadenza/presentation/generated/timer_digits_sharp.h \
  --symbol kTimerDigitsSharp \
  --provenance "Cadenza-authored industrial Timer numerals; native 716x120 atlas from tools/generate_timer_digits.py"
```

封面裁切、灰阶缩略图和 reflective preview 使用 `tools/asset-requirements.txt` 中固定版本的 Pillow；该依赖仅属于离线资产流程。
