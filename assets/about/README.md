# Cadenza OS About identity

The About screen uses the user-approved grand-piano identity lockup with the
exact wordmark `CADENZA OS` and slogan `A Space to Improvise.`.

- Approved baseline: 2026-07-19; whitespace-cropped v6 enlargement applied at
  user request
- Source: `source/about_logo.png`
- Sharp asset: `about_logo.pbm` (350×155)
- T-Embed asset: `about_logo_t_embed.pbm` (280×124)
- Conversion: three tonal bands with explicit antialias edge cleanup

Regenerate the formal assets and packed headers with:

```bash
python3 .codex/skills/generate-launcher-cover-art/scripts/prepare_cover.py \
  assets/about/source/about_logo.png assets/about --name about_logo \
  --levels 3 --edge-cleanup --pack-tool tools/pack_bitmap.py \
  --header-dir lib/cadenza_apps/src/generated
```

The complete pre-approval prompt, rejected concepts, master and 1-bit review are
retained under
`assets/launcher-covers/review/2026-07-19-about/about/`.
