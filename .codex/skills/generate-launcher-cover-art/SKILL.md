---
name: generate-launcher-cover-art
description: Generate, review, convert, and optionally integrate CadenzaOS Launcher Cover illustrations in the project's retro-futurist scientific-instrument visual system. Use when adding or iterating CLOCK, MOTION, SETTINGS, ANIMATION GALLERY, fallback, or a new App Cover; producing smooth grayscale master PNGs; converting them to reviewed 350×155 and 280×124 1-bit assets; packing C++ headers; or applying an approved Cover to its App. Do not approve visual baselines without explicit user approval.
---

# Generate Launcher Cover Art

Create coherent App identity illustrations and carry each approved image through
the project's 1-bit pipeline. Treat every Cover as one static, non-interactive
image.

## Required reference

Read [references/cover-art-direction.md](references/cover-art-direction.md) in
full before composing prompts or reviewing results. Use its shared style suffix,
negative prompt, App-specific scene brief, and acceptance checklist.

## Workflow

1. Confirm the requested App set and output directory from the task or current
   project context. Inspect existing assets without modifying code.
2. Check ownership before writing. Never overwrite images or source files being
   changed by another task. Generate into a new review/master directory unless
   the user names an exact destination.
3. Generate and apply one App at a time. Assemble the image prompt as:

   ```text
   App-specific scene brief
   + shared visual style suffix
   + negative prompt as explicit exclusions
   + exact title spelling and 350:155 composition requirement
   ```

4. Prefer a smooth lossless PNG master at 1400×620; use 700×310 when the larger size
   is unavailable. If the generator returns another size, preserve that original
   and do not claim it is delivery-ready until an explicit 350:155 crop has been
   reviewed.
5. Review the actual pixels, not just the prompt. Reject and regenerate text
   misspellings, clipped titles, extra panels, external frames, tiny details,
   filled MOTION balls, or inconsistent light/dark balance.
6. When generating a set, use an accepted earlier cover as a visual reference
   where the image tool supports it. Keep typography character, safe margins,
   line weight, and editorial rhythm consistent without duplicating layouts.
7. After the user accepts the composition, locate a Python runtime with Pillow
   (use Codex bundled workspace dependencies when system Python lacks it), then
   run `scripts/prepare_cover.py` to preserve the PNG and create 350×155 plus
   no-crop 280×124 PBMs and pure/reflective previews. Use 3–6 intentional tonal
   bands; start with 5.
8. Review both real-size previews. Manually iterate the master when title edges,
   two-pixel strokes, dominant silhouettes, or dither fields become noisy. An
   automatic conversion is a candidate, never an approval.
9. When adding the Cover end to end, pass the repository `tools/pack_bitmap.py`
   and generated-header directory, wire the two profile bitmaps into the App's
   const renderer, run asset checks, interaction immutability tests, snapshots,
   and the firmware size gate. Finish one App before starting the next.

```bash
"$IMAGE_PYTHON" .codex/skills/generate-launcher-cover-art/scripts/prepare_cover.py \
  /tmp/clock.png assets/launcher-covers --name clock --levels 5 \
  --pack-tool tools/pack_bitmap.py \
  --header-dir lib/cadenza_apps/src/generated
```

## Static Cover contract

- Never generate Idle, Highlighted, Pressed, or Launching Cover variants.
- Never animate or alter Clock time, Motion geometry, or any other internal
  pixels because of selection, press, long press, or launch.
- Treat launch animation as a separate future asset and product contract.
- Do not draw card borders, button hints, page dots, or Launcher navigation;
  Launcher owns those elements.

## Set-level art direction

- Give the title roughly 20–30% of visual weight and keep it inside safe margins.
- Use one core scene and one dominant geometric object per Cover.
- Make CLOCK and ANIMATION GALLERY dark; make MOTION and SETTINGS light.
- Build the master with smooth high-resolution contours and 3–5 deliberate gray
  values. Allow antialiasing in the master; never ask the generator to pre-bake
  jagged pixel stair-steps.
- Use gray structurally: for example, a title may use a lighter/dithered upper
  face and a solid lower face or side plane to create depth. Keep the letter
  silhouette and baseline solid enough to read without the gray treatment.
- Avoid continuous photographic gradients. Translate designed gray planes into
  regular ordered dither, then clean noisy edge pixels by hand.
- Keep critical generated contours at least 4–8 pixels wide so they survive
  reduction to 350×155.
- Integrate the exact App name into the illustration. Do not depend on Launcher
  to add another title.

## Handoff

Return clickable absolute paths for every original PNG, both PBMs, and pure plus
reflective previews. State which checklist items passed or still need iteration,
and report packed-header, snapshot, and firmware-size results when integration
was requested. Do not call an image or hash “approved” without an explicit visual
review decision.
