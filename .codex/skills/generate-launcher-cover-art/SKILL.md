---
name: generate-launcher-cover-art
description: "Generate, simplify, review, convert, and optionally integrate Playdate-literate CadenzaOS Launcher Cover illustrations: highly legible 1-bit micro-posters with large titles, an App-specific functional hook, strong black/white recognition, and controlled gray planes that add depth and fun within a coherent restrained industrial style. Use when adding or iterating TIMER, MOTION, SETTINGS, ANIMATION GALLERY, fallback, or a new App Cover; correcting a generic, unclear, flat, or over-detailed Cover; producing smooth grayscale master PNGs; converting them to reviewed 350×155 and 280×124 1-bit assets; packing C++ headers; or applying an approved Cover to its App. Do not approve visual baselines without explicit user approval."
---

# Generate Launcher Cover Art

Create App identity illustrations that share Playdate's medium grammar without
imitating a single Catalog title. A consistent restrained, cool, or industrial
house style is welcome; each Cover must still communicate its App through a
specific functional metaphor and an interesting visual idea. Carry each approved
image through the project's 1-bit pipeline. Treat every Cover as one static,
non-interactive image.

Resolve art decisions in this order: title readability, App-function recognition,
one interesting visual relationship, then house-style consistency. Never sacrifice
an earlier priority to improve a later one.

## Required reference

Read [references/cover-art-direction.md](references/cover-art-direction.md) in
full before composing prompts or reviewing results. Use its optional visual-strategy module,
shared 1-bit medium-grammar suffix, negative prompt, App-specific scene brief, and
acceptance checklist.

## Workflow

1. Confirm the requested App set and output directory from the task or current
   project context. Inspect the App, existing assets, and any supplied visual
   references without modifying code.
2. Write a one-sentence identity brief before prompting: what the App does, the
   one visual metaphor that makes that function obvious and interesting, the
   title's role in the composition, and whether the image is predominantly dark
   or light. Treat title readability and functional recognition as higher
   priorities than stylistic novelty.
3. Select one visual strategy from the required reference when it helps. The set
   may reuse the same restrained industrial dialect, type system, and tonal
   manner. Do not force variety for its own sake, combine several dialects by
   default, or reproduce a specific Playdate game's lettering, character, or
   layout.
4. Check ownership before writing. Never overwrite images or source files being
   changed by another task. Generate into a new review/master directory unless
   the user names an exact destination.
5. Define a strict element budget before prompting: the exact title, one dominant
   motif, and at most one supporting shape or directional device. The motif may
   be an object, character, symbol, or the title itself. Add zero to two small
   accents only when they communicate App identity. List the permitted elements
   explicitly; instruct the generator not to invent anything else. Also define a
   tonal plan: the main black mass, main white mass, and one to three broad gray
   planes, with a spatial or material purpose for every gray. When using 2D/3D
   overlap, also state the occlusion order, tilt, front face, and side face.
6. Generate and apply one App at a time. Assemble the image prompt as:

   ```text
   identity brief and App-specific scene brief
   + selected visual-strategy module when useful
   + explicit permitted-element list
   + shared 1-bit medium-grammar suffix
   + negative prompt as explicit exclusions
   + exact title spelling and 350:155 composition requirement
   ```

7. Prefer a smooth lossless PNG master at 1400×620; use 700×310 when the larger size
   is unavailable. If the generator returns another size, preserve that original
   and do not claim it is delivery-ready until an explicit 350:155 crop has been
   reviewed.
8. Review the composition first as a hard-threshold pure black/white thumbnail,
   not only as a grayscale image. The exact title and one memorable hook must
   remain immediately recognizable before any gray texture is credited. If they
   do not, change the massing or composition; never use dither to rescue them.
   Then review the actual pixels at 350×155 and 280×124, not just the prompt or
   enlarged master. Reject misspellings, clipped titles, generic dashboard
   decoration, extra panels, external frames, tiny details, inconsistent
   light/dark balance, copied-looking motifs, or any unbudgeted prop. Remove
   details that disappear at 280×124 or compete with the title and dominant
   silhouette; never retain them merely because they look attractive enlarged.
9. When generating a set, use an accepted earlier Cover to calibrate the house
   style as well as safe margins, minimum contour weight, reduction quality, and
   texture density. Reusing a cool industrial dialect or readable type system is
   acceptable. Keep the App-specific functional metaphor and visual hook distinct
   enough that Covers are not interchangeable.
10. After the user accepts the composition, locate a Python runtime with Pillow
   (use Codex bundled workspace dependencies when system Python lacks it), then
   run `scripts/prepare_cover.py` to preserve the PNG and create 350×155 plus
   no-crop 280×124 PBMs and pure/reflective previews. Use 3–6 intentional tonal
   bands; start with 5.
11. Review both real-size previews. Manually iterate the master when title edges,
   two-pixel strokes, dominant silhouettes, or dither fields become noisy. An
   automatic conversion is a candidate, never an approval.
12. When adding the Cover end to end, pass the repository `tools/pack_bitmap.py`
   and generated-header directory, wire the two profile bitmaps into the App's
   const renderer, run asset checks, interaction immutability tests, snapshots,
   and the firmware size gate. Finish one App before starting the next.

```bash
"$IMAGE_PYTHON" .codex/skills/generate-launcher-cover-art/scripts/prepare_cover.py \
  /tmp/timer.png assets/launcher-covers --name timer --levels 5 \
  --pack-tool tools/pack_bitmap.py \
  --header-dir lib/cadenza_apps/src/generated
```

## Static Cover contract

- Never generate Idle, Highlighted, Pressed, or Launching Cover variants.
- Never animate or alter Timer time, Motion geometry, or any other internal
  pixels because of selection, press, long press, or launch.
- Treat launch animation as a separate future asset and product contract.
- Do not draw card borders, button hints, page dots, or Launcher navigation;
  Launcher owns those elements.

## Simplification contract

- Treat a Cover as a micro-poster and identity mark, not a miniature environment
  illustration or a screenshot of the App UI.
- Limit the composition to three semantic layers: title, one dominant motif, and
  one broad supporting shape such as a beam, track, panel, or motion path.
- Permit at most two small accents, and only when removing one would weaken App
  recognition. Do not use accents merely to fill negative space.
- Preserve one broad quiet field of black, white, or a single controlled texture.
  Do not distribute detail evenly across the canvas.
- Delete micro-architecture, repeated hardware, decorative particles, star fields,
  debris, skyline clutter, tiny instruments, and ornamental markings unless they
  are the dominant App-defining object.
- Use no more than one dominant dither material plus one small secondary material.
  Keep title counters, focal silhouettes, and critical boundaries solid.
- Judge complexity at 1× output size. If a feature does not read cleanly at both
  350×155 and 280×124, enlarge it into a primary shape or remove it.

## Set-level art direction

- Integrate the title as artwork, not as a caption pasted onto a scene. Let it
  occupy roughly 30–50% of visual weight by default; a typography-led Cover may
  make the title the dominant motif. At 350×155, target a main-title cap height of
  at least 24 pixels; a long stacked title may use 18 pixels per line only when
  every letter remains unmistakable at 1×.
- Use one core scene and one dominant motif per Cover. Do not require every motif
  to be geometric or every title to use the same type treatment.
- Make TIMER and ANIMATION GALLERY dark; make MOTION and SETTINGS light.
- Build the master with smooth high-resolution contours and 3–5 deliberate gray
  values. Allow antialiasing in the master; never ask the generator to pre-bake
  jagged pixel stair-steps.
- Use gray structurally: for example, a title may use a lighter/dithered upper
  face and a solid lower face or side plane to create depth. Keep the letter
  silhouette and baseline solid enough to read without the gray treatment.
- Use broad gray planes to make the sparse composition feel dimensional and fun:
  define a front face, side face, overlap, cast shape, beam, or atmosphere. Every
  gray must clarify volume, separation, motion, or material; delete ornamental
  gray that only makes the image busier.
- Permit a flat title to overlap a tilted volumetric object. Keep the title on a
  clean 2D plane and use only a front face, rim, and side wall for the 3D object;
  the occlusion must remain obvious after hard threshold and at 280×124.
- Avoid continuous photographic gradients. Translate designed gray planes into
  regular ordered dither, then clean noisy edge pixels by hand.
- Prefer large flat planes over rendered machinery. Grayscale may make one object
  feel dimensional, but must not create additional layers of incidental detail.
- Keep critical generated contours at least 4–8 pixels wide so they survive
  reduction to 350×155.
- A shared restrained industrial style, font family, or scientific-instrument
  vocabulary is acceptable. Build interest through App-specific metaphor,
  surprising scale, crop, overlap, motion, or gray-plane depth rather than through
  decoration or forced stylistic variety.
- Never distort a title merely to make it “integrated.” A straightforward family
  type treatment is preferable when it preserves immediate reading; integrate it
  through scale, placement, contrast, or interaction with the motif.
- Integrate the exact App name into the illustration. Do not depend on Launcher
  to add another title.

## Handoff

Return clickable absolute paths for every original PNG, both PBMs, and pure plus
reflective previews. State which checklist items passed or still need iteration,
and report packed-header, snapshot, and firmware-size results when integration
was requested. Do not call an image or hash “approved” without an explicit visual
review decision.
