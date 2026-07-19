# CadenzaOS Launcher Cover art direction

## Evidence and intent

Base the system on the medium constraints in *Designing for Playdate*, SDK
1.12.3, and on the recurring visual grammar visible across Playdate Launcher and
Catalog cards. Do not treat “Playdate style” as a bag of retro decorations. Its
most reusable qualities are immediate black/white recognition, large readable
titles, one memorable idea, and purposeful gray texture.

Borrow that grammar, not any game's protected character, logo, lettering, or
layout. CadenzaOS Covers should feel native to the same design culture while
remaining original and recognizably CadenzaOS.

## Shared system

Unify visible Launcher Apps as restrained industrial 1-bit micro-posters: a large
and immediately readable title, one functional visual hook, broad black/white
relationships, and purposeful gray planes. Treat each Cover as an identity mark
rather than a miniature environment or UI screenshot. The whole set may be cool,
minimal, industrial, dark, or use one readable type system. Sameness becomes a
problem only when the Covers stop communicating what their Apps do or stop being
interesting.

Use this strict priority order whenever requirements compete:

1. read the exact title immediately;
2. understand what the App does;
3. remember one interesting visual relationship;
4. maintain the restrained industrial house style.

Target a strict 350:155 landscape composition. Generate masters at 1400×620 or
700×310. Draw no external card frame; Launcher supplies it. Evaluate visual
complexity on the physical 1-bit target, never from the enlarged master alone.

The physical constraints are derived from *Designing for Playdate*, SDK 1.12.3:
the 400×240 reflective 1-bit screen depends on area, contour, spacing, and deliberate
texture; important strokes should be at least 2 pixels; automated dithering can
become fuzzy; and high-frequency patterns can flash when shifted by one pixel.

## Playdate cover grammar

Apply all of these principles before selecting an optional visual strategy:

1. **Micro-poster, not screenshot.** Communicate identity and mood in one glance;
   do not explain every feature or reproduce the App interface.
2. **Large, readable title as image.** Compose the exact App name into the art and
   normally give it roughly one third to one half of the visual weight. Its scale,
   shape, rhythm, or interaction with the motif should convey personality, but
   readability wins over novelty.
3. **One recall hook.** Make the Cover describable as “the one with the ___.” The
   hook may be a mascot, object, symbol, gesture, or distinctive wordmark.
4. **Black/white recognition before texture.** The exact title and recall hook
   must remain recognizable after a hard threshold to pure black and white.
   Dither may describe material or atmosphere but must not rescue weak massing,
   thin lettering, or an ambiguous silhouette.
5. **Gray as designed volume, not conversion noise.** Use a few broad gray planes
   to create a front face, side face, overlap, cast shape, motion layer, material,
   or atmosphere. Render them with controlled halftone, hatching, stripes, or
   ordered dither, never as generic gray dust.
6. **Function-specific idea.** A viewer should be able to connect the main motif
   to the App's purpose: time, movement, control, animation, or another concrete
   function. Reject generic machinery that could be reassigned to any App.
7. **Interest inside restraint.** A uniform cool industrial house style is valid.
   Create interest with one unexpected relationship—scale, crop, overlap,
   anthropomorphism, motion, or dimensional gray planes—not with more objects.

## Visual dialect modules

Use these as optional compositional strategies, not a mandate that every App use
a different style. A set may stay entirely within editorial-minimal or industrial
software-package language. Append at most one module when it helps, then adapt it
to the App instead of copying a known game.

### Typographic signal

Use when the App name or motion can carry the identity. Make the custom wordmark
the dominant shape; let scale, slant, compression, extrusion, repetition, or one
supporting symbol express the App. Keep illustration subordinate and preserve
open counters at output size.

### Mascot or comic emblem

Use when a character or anthropomorphic object can embody the App. Show one
large, instantly readable pose with an expressive silhouette and economical
interior marks. Favor editorial-cartoon clarity over a detailed game scene.

### Editorial minimal

Use for calm, exact, mysterious, or conceptual Apps. Combine generous negative
space, one symbolic object, restrained typography, and at most one rule, beam, or
plane. Let asymmetry and scale create tension; do not fill empty space.

### Narrative vignette

Use when a single dramatic action communicates the App. Frame one large subject
and one directional force like a film still or pulp illustration. Crop boldly,
but do not add scenery, secondary characters, or a complete environment.

### Print-zine expression

Use for playful, handmade, noisy, or experimental Apps. Permit hand-drawn
lettering, chunky hatching, rough ink edges, or one concentrated halftone field.
Keep the title and main silhouette controlled; “rough” must not mean illegible or
uniformly speckled.

### Retro software package

Use for technical, strategic, or system-oriented Apps. Use a strong logo lockup,
one emblem, disciplined alignment, and optionally one short semantic micro-label
only when it remains legible. Avoid turning the Cover into a dashboard or copying
a real platform's trade dress.

### Dimensional overlap

Use when a flat title and one functional object can create a memorable 2D/3D
relationship. Keep the title frontal and immediately readable; place one tilted
volumetric object behind or through it, and define an explicit occlusion order.
Build the object from no more than a front face, rim, and side wall. Assign each
gray plane to one of those surfaces so perspective survives ordered-dither
conversion. Reject generic extruded decoration, floating objects with no App
meaning, photorealistic rendering, and any overlap that obscures letter identity.
At hard threshold, the title, object silhouette, and which element is in front
must still be obvious.

## Visual complexity budget

Before composing a prompt, write a permitted-element list containing only:

1. the exact App title;
2. one dominant App-defining motif;
3. one broad supporting shape or directional device;
4. zero to two small semantic accents.

Require the generator to use only that list. A small accent must communicate App
identity when viewed at 350×155; empty-space decoration does not qualify. Keep one
broad quiet field and concentrate detail around the focal silhouette. If a detail
cannot survive at 280×124 with an open shape and stable pixels, remove it rather
than sharpening or outlining it.

Use a three-pass deletion review:

1. Remove every object not required for App recognition.
2. Remove internal marks that do not change the dominant silhouette.
3. Remove texture that weakens the title, focal object, or broad black/white read.

### Shared 1-bit medium-grammar suffix

Append this text to every App prompt:

```text
original restrained industrial 1-bit micro-poster identity art for a reflective monochrome handheld, exact app title integrated as a large immediately legible primary graphic occupying roughly one third to one half of the composition, one functional visual metaphor that makes the App purpose obvious, one instantly memorable and interesting visual relationship, one broad supporting relationship, bold black and white massing that preserves both title and hook under a hard pure-black-and-white threshold before any texture, large legible silhouettes, purposeful editorial composition, radical visual economy, coherent cool industrial house style allowed, high-resolution smooth master designed for later 1-bit ordered-dither conversion, black white and three to five deliberate gray values used only as broad planar surfaces that clarify front face side face overlap cast shape motion material or atmosphere, dimensional and playful through scale overlap and volume rather than decorative clutter, restrained print-like texture rather than photographic shading, clean antialiased master edges, use only the explicitly permitted elements and invent no additional props, no card border, no rounded container, landscape 350:155 aspect ratio, title fully inside safe margins
```

### Negative prompt

Apply these as explicit exclusions:

```text
Exclude: imitation of a specific Playdate game cover, copied logo or character, generic “retro game” collage, industrial decoration with no App-specific meaning, photorealistic, studio product photography, smooth 3D product render, glossy UI, cyberpunk neon, colorful, soft drop shadows, contact shadows, ambient occlusion, specular highlights, glossy bevels, continuous photographic gradients, blur, intentionally jagged pixel-font stair-steps in the master, thin lines, tiny text, dense dashboard, generic mobile app UI, multiple panels, external frame, rounded card border, miniature architecture, cables, bolts, screws, railings, antennas, star fields, skyline clutter, debris, decorative particles, extra icons, ornamental instruments, speckle, random texture, invented background props, Playdate logo, copyrighted characters.
```

## Typography and grayscale

- Treat title design as part of concept development. Avoid placing the same neutral
  font at the same coordinate on every Cover without considering the motif. A
  shared family type treatment is valid and often improves readability. Integrate
  it through scale, placement, contrast, or overlap; do not require ornamental or
  distorted lettering.
- Default to a title occupying roughly 30–50% of the Cover's visual weight. At
  350×155, target a cap height of at least 24 pixels for the main title; allow a
  long stacked title to use 18 pixels per line only after verifying every letter
  at 1×. Never trade readable counters and letter distinction for stylistic effect.
- Design title glyphs as smooth high-resolution shapes first. Pixel hinting and
  1-bit cleanup happen after reduction; do not confuse “bitmap-inspired” with
  deliberately rough diagonal and curved outlines.
- Treat master antialiasing and final gray material as different signals. During
  conversion, use `prepare_cover.py --edge-cleanup` to harden soft pixels that
  bridge near-black and near-white silhouettes and its one-pixel guard band.
  Keep broad gray planes outside that contour guard eligible for ordered dither.
- Inspect a nearest-neighbor 4× rendering after conversion. Regular monotonic
  pixel stair steps are the 1-bit contour; Bayer dots, comb teeth, isolated
  pinholes, and fuzzy patterned halos touching the contour are defects.
- Keep final important strokes at least 2 pixels thick. Use generous cap height,
  clear counters, distinguishable letters, and manual cleanup after conversion.
- Use gray as a small material vocabulary, not generic noise: highlight face,
  side plane, metal, atmosphere, or depth. A strong title treatment is a
  dithered/light upper face over a solid black lower face or extrusion.
- Write a tonal plan before generation: name the main black mass, the main white
  mass, and one to three broad gray planes. Assign each gray a job—volume,
  separation, overlap, motion, material, or atmosphere—and remove any plane that
  has no such job. Prefer large face/side/extrusion relationships and bold cast
  shapes over many small shaded details.
- Preserve a solid silhouette around critical text. Gray may add volume but must
  not be required to decipher the word.
- Prefer ordered, repeatable patterns and 3–5 tonal bands. Avoid broad fields of
  automatic error-diffusion noise, especially around titles and focal objects.
- Use at most one dominant patterned material and one smaller secondary material.
  Prefer broad, low-frequency patterns; avoid 2×2 checker fields whose phase can
  invert during one-pixel Launcher motion.
- Use gray to describe one large face, side plane, beam, ground, or atmosphere.
  Never spend separate gray values on many tiny parts.
- Because the whole Cover moves in Launcher, inspect gray patterns during
  one-pixel animation. Reduce or coarsen patterns that shimmer or flash.

## App briefs

### TIMER

Purpose: countdown timer; rotary input adjusts time and Confirm pauses/resumes.
Art role: the set's dark, exact 2D/3D industrial micro-poster.

```text
A radically simplified dimensional-overlap launcher cover for an app titled “TIMER”. The first impression is dark, exact, and dimensional; the recall hook is the complete foreground letter R occluding one large tilted 3D countdown dial behind it. Use only the word “TIMER” and that dial. Keep the title flat, frontal, bright, and immediately readable. Build the dial from a light rim, one broad gray face, one dark side wall, one simple hand, and one missing sector that makes elapsed time obvious. Preserve the complete R silhouette in front of the dial. Use three to five deliberate gray values whose ordered-dither conversion retains the face/rim/side-wall depth. Do not add clock towers, small ticks, numerals, multiple hands, buttons, screws, shadows detached from the dial, secondary instruments, or a miniature Timer UI. Spell the title exactly “TIMER”.
```

Keep the displayed time static in all Launcher interactions.

### MOTION

Purpose: spring/inertia experiment; rotary input changes the target and a circle
tracks it elastically. Art role: the bright kinetic laboratory.

```text
A radically simplified typographic-signal launcher cover for an app titled “MOTION”. The first impression is bright, playful, and kinetic; the recall hook is one large hollow circle visibly pulling the wordmark along a track. Use only the word “MOTION”, that hollow circular object, one horizontal track, one target marker, and at most three broad speed marks. Let the lettering lean, stretch, or rhythmically compress in the direction of travel without harming legibility. Do not combine a grid, particles, multiple arrow systems, measurement ticks, or secondary instruments. Use a mostly white background with bold black geometry and generous quiet space. The focal object must remain hollow white inside, never a large solid black ball. Spell the title exactly “MOTION”.
```

### SETTINGS

Purpose: control center for motion intensity, sound, Wi-Fi/provisioning and
Launcher orientation. Art role: a calm, authoritative, mostly white precision
console—not a settings list.

```text
A radically simplified retro-software-package launcher cover for an app titled “SETTINGS”. The first impression is calm, authoritative, and approachable; the recall hook is one grouped tactile control deck. Use only the word “SETTINGS” and that single deck on one common base plane, containing three large silhouettes: a rotary dial, a physical toggle, and a horizontal slider. Give each control one bold state-defining mark and no ornamental hardware. Build a distinctive logo lockup rather than a conventional settings-screen heading. Use a mostly white background, orderly spacing, and generous quiet space. Do not add connecting paths, Wi-Fi symbols, screws, labels, calibration ticks, extra panels, or a conventional settings list. Spell the title exactly “SETTINGS”.
```

### ANIMATION GALLERY

Purpose: demonstrate easing, spring, transition, particles, camera, sprite state
and dither. Art role: the dark but active experimental stage.

```text
A radically simplified print-zine launcher cover for an app titled “ANIMATION GALLERY”. The first impression is experimental, theatrical, and alive; the recall hook is one figure appearing in exactly three successive poses along one continuous path. Use only the stacked title “ANIMATION GALLERY”, that repeated core figure, and one easing arc or spring path. Permit one concentrated halftone or hatching field, but keep the title and figures crisp. The sequence and path must express transformation without becoming a technical chart. Use a theatrical black background with generous empty space. Do not add particles, frame numbers, chart axes, multiple curves, camera marks, secondary icons, or unrelated function diagrams. Spell the title exactly “ANIMATION GALLERY”.
```

## Acceptance checklist

Reject or iterate unless every applicable item passes:

- Canvas composition is exactly 350:155 or safely crop-ready to that ratio.
- Exact App title is legible, fully inside safe margins, and normally carries
  30–50% of visual weight.
- Main-title cap height is at least 24 pixels at 350×155, or at least 18 pixels per
  line for a verified long stacked title; final important strokes remain at least
  2 pixels thick and counters stay open.
- The title looks conceived with the image rather than pasted on as a caption.
- The Cover makes the App's function evident through one specific visual metaphor;
  its motif would not fit another App equally well.
- The Cover contains one interesting relationship—scale, crop, overlap, motion,
  anthropomorphism, or dimensional gray planes—without adding clutter.
- The Cover contains one primary scene and one dominant motif.
- The Cover stays within three semantic layers: title, dominant motif, and one
  broad supporting shape; there are no more than two small semantic accents.
- TIMER/GALLERY read as dark and MOTION/SETTINGS read as light.
- The chosen house style or visual strategy is coherent and does not imitate a
  specific Catalog title, copyrighted character, logo, or layout.
- Safe margins, contour weight, title readability, reduction quality, and texture
  discipline form one family; stylistic uniformity is not rejected by itself.
- A hard-threshold pure black/white thumbnail preserves both the exact title and
  recall hook before any dither is credited.
- Before conversion, the grayscale master reads as deliberate print illustration,
  paper-cut art, or a technical poster built from three to five discrete planes;
  it does not depend on studio lighting, soft shadows, specular highlights,
  ambient occlusion, glossy bevels, or continuous gradients for its form.
- The dominant object's outer silhouette is a continuous solid boundary that
  remains clear without gray texture; `--edge-cleanup` is not used to rescue a
  photographic or weakly separated master.
- No external border, rounded card, page indicator, input hint or Launcher chrome.
- Master contours are smooth and antialiased; final 1-bit contours are manually
  hinted with no isolated stair-step noise, Bayer fringe, comb teeth, pinholes,
  or patterned gray halo touching a critical boundary.
- Every newly approved Cover was prepared with explicit `--edge-cleanup`; a 4×
  nearest-neighbor contour review confirms clean regular stair steps while broad
  structural gray planes still retain intentional ordered dither.
- Gray planes add intentional hierarchy or volume and survive as controlled
  ordered dither; there are no broad fuzzy auto-dither fields.
- Every gray plane has a documented role in volume, separation, overlap, motion,
  material, or atmosphere; gray makes the composition more dimensional or
  expressive without weakening pure-black-and-white recognition.
- Important final strokes are at least 2 pixels thick and counters remain open
  at both 350×155 and 280×124.
- No tiny labels or dense dashboard structures; technical decoration appears only
  when required by the selected dialect and App identity.
- No decorative micro-architecture, star field, skyline clutter, debris, repeated
  hardware, or texture used merely to fill space.
- Important contours remain readable after a 4× reduction to 350×155.
- Every retained detail remains meaningful and stable at both 350×155 and
  280×124 when viewed at 1×; enlarged-master appeal is not an acceptance reason.
- MOTION's core circle is hollow; TIMER has one tilted countdown dial and a
  complete foreground `R`.
- Image is a static identity Cover, not a pressed/selected/launch variant.
- Original lossless PNG is retained separately from derived crops and 1-bit files.
- Pure black/white and reflective-palette previews are reviewed at 1× size.
- A short moving-track review shows no objectionable dither shimmer or flashing.
