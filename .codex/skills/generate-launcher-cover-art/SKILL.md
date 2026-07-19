---
name: generate-launcher-cover-art
description: "Generate, simplify, review, de-fringe, convert, and optionally integrate Playdate-literate CadenzaOS Launcher Cover illustrations: highly legible 1-bit micro-posters with large titles, an App-specific functional hook, strong black/white recognition, clean solid contours, and controlled gray planes that add depth and fun within a coherent restrained industrial style. Use when adding or iterating TIMER, MOTION, SETTINGS, ANIMATION GALLERY, fallback, or a new App Cover; correcting a generic, unclear, flat, over-detailed, jagged, or dither-fringed Cover; producing smooth grayscale master PNGs; converting them to reviewed 350×155 and 280×124 1-bit assets; packing C++ headers; or applying an approved Cover to its App. Do not approve visual baselines without explicit user approval."
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

## Gated pipeline

Treat the workflow as a state machine. An asset moves through `brief → master
candidate → pixel candidate → user-approved baseline → integrated → verified`.
Never let deterministic conversion success stand in for a subjective visual
gate, and never overwrite formal assets from a pre-approval state. User feedback
may return the asset to any earlier state.

### Stage 0 — scope and ownership

Confirm the exact App and review root, inspect its behavior and current assets,
and check whether another task owns any overlapping files. Work on one App at a
time. Put every unapproved result under a new dated `review/<change>/<app>/`
directory; keep formal `source/`, PBMs, headers, and goldens untouched.

### Stage 1 — brief and master generation

Write a one-sentence identity brief covering App purpose, one functional metaphor,
the title's compositional role, and light/dark balance. Choose at most one visual
strategy from the required reference. Define before prompting:

- exact title;
- one dominant motif and one broad supporting relationship;
- zero to two semantic accents;
- main black and white masses plus one to three named gray planes;
- a layout decision made for this App, not inherited from the previous Cover.

Assemble the prompt from that brief, the App scene brief, permitted elements,
shared medium suffix, explicit negative prompt, exact spelling, and 350:155 ratio.
Generate a smooth lossless PNG, preferably 1400×620. Preserve the generator's
original separately and make any crop or resize explicit.

### Gate A — illustration master

Review the master before conversion. Reject it immediately if any of these fail:

- the title is exact, inside safe margins, and integrated with the concept;
- the functional hook and dominant silhouette read without texture;
- the image uses only budgeted elements and a layout distinct from the set's
  accidental templates;
- the form is deliberately illustrated with three to five discrete hard-boundary
  planes and a continuous solid outer silhouette;
- it does not depend on studio lighting, continuous gradients, soft contact
  shadows, ambient occlusion, specular highlights, glossy bevels, or other
  photo/product-render cues.

Do not ask dither or edge cleanup to rescue a failed master. Record the failure
and iterate the source concept.

### Stage 2 — deterministic pixel candidate

Run `scripts/prepare_cover.py` in the candidate directory with 3–6 tonal bands
(start with 5) and explicit `--edge-cleanup`. The script produces both profiles,
PBMs, ordered-dither pure/reflective previews, texture-independent hard-threshold
previews, and nearest-neighbor 4× inspection previews. Edge cleanup hardens
antialiased black/white transitions and a one-pixel guard while preserving broad
structural gray planes. Keep the flag explicit so legacy assets do not re-hash.

### Gate B — real pixel review

Review, in this order, the hard-threshold 350×155 and 280×124 previews at 1×, the
ordered-dither reflective previews at 1×, and the 4× nearest-neighbor previews.
Reject clipped or misspelled titles, collapsed counters, disappearing details,
weak silhouettes, excessive texture, Bayer dots, comb teeth, pinholes, or fuzzy
patterned halos touching critical contours. Regular monotonic 1-bit stairs are
not defects. If the pixel result materially changes the accepted master, return
it to the user for another explicit visual decision.

### Gate C — explicit user baseline approval

Show the actual pixel candidate, not only the enlarged master. Record the user's
explicit approval with the candidate path and date. Generation, conversion,
tests, hashes, silence, or approval of an earlier revision do not approve the
current pixel baseline.

### Stage 3 — formal integration and verification

Only after Gate C, copy the approved master into formal `source/`, regenerate
canonical PBMs and packed headers with the same parameters, and update provenance.
Run source reproducibility, PBM/header, dimensions, tonal, immutability, handoff,
snapshots, moving-track dither, host, desktop/headless, firmware-compatible build,
and size-gate checks. Audit untouched Cover hashes and the final diff. A failed
verification returns to the owning stage; do not patch runtime rendering to fix
source-art defects.

```bash
"$IMAGE_PYTHON" .codex/skills/generate-launcher-cover-art/scripts/prepare_cover.py \
  /tmp/new-cover.png assets/launcher-covers --name new-cover --levels 5 \
  --edge-cleanup \
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
- Never let ordered dither consume antialiasing pixels on critical black/white
  contours. Use the pipeline edge-cleanup guard, then inspect at 4× for irregular
  Bayer fringe; do not confuse clean regular pixel stair steps with defects.
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
- Avoid continuous photographic gradients and product-render lighting. Translate designed gray planes into
  regular ordered dither. Harden antialiased silhouette edges with the pipeline
  cleanup before any remaining intentional hand hinting.
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
including the 1× and 4× contour inspection and whether `--edge-cleanup` was used.
Report packed-header, snapshot, and firmware-size results when integration was
requested. Do not call an image or hash “approved” without an explicit visual
review decision.
