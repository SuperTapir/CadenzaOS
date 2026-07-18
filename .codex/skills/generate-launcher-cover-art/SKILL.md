---
name: generate-launcher-cover-art
description: Generate or iterate CadenzaOS Launcher cover illustrations with GPT Image in the project's retro-futurist scientific-instrument visual system. Use when creating CLOCK, MOTION, SETTINGS, ANIMATION GALLERY, fallback, or new App cover master PNGs; reviewing cover consistency; or preparing approved masters for later 350×155 and 1-bit conversion. Do not use this skill to change Launcher code or approve visual baselines unless the user explicitly requests implementation or approval.
---

# Generate Launcher Cover Art

Create coherent App identity illustrations that remain readable after 1-bit
conversion. Treat every Cover as one static, non-interactive image.

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
3. Generate one App at a time with GPT Image. Assemble the prompt as:

   ```text
   App-specific scene brief
   + shared visual style suffix
   + negative prompt as explicit exclusions
   + exact title spelling and 350:155 composition requirement
   ```

4. Prefer a lossless PNG master at 1400×620; use 700×310 when the larger size
   is unavailable. If the generator returns another size, preserve that original
   and do not claim it is delivery-ready until an explicit 350:155 crop has been
   reviewed.
5. Review the actual pixels, not just the prompt. Reject and regenerate text
   misspellings, clipped titles, extra panels, external frames, tiny details,
   filled MOTION balls, or inconsistent light/dark balance.
6. When generating a set, use an accepted earlier cover as a visual reference
   where the image tool supports it. Keep typography character, safe margins,
   line weight, and editorial rhythm consistent without duplicating layouts.
7. Deliver the original lossless PNGs first. Only crop, threshold, dither,
   convert to 1-bit, pack assets, update hashes, or modify C++ when the user
   explicitly requests that separate step.

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
- Use black, white, and at most two gray levels. Avoid gradients and antialiasing.
- Keep critical generated contours at least 4–8 pixels wide so they survive
  reduction to 350×155.
- Integrate the exact App name into the illustration. Do not depend on Launcher
  to add another title.

## Handoff

Return clickable absolute paths for every master PNG and state which checklist
items passed or still need iteration. Clearly distinguish generated masters,
review crops, and final 1-bit assets. Do not call an image “approved” without an
explicit visual review decision.
