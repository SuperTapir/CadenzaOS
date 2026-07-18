# CadenzaOS Launcher Cover art direction

## Shared system

Unify visible Launcher Apps as one “retro-futurist experimental instrument /
scientific field manual” product family: strong titles, one dominant object,
broad black/white relationships, and very few purposeful details. Treat each
Cover as an identity mark rather than a miniature environment. Do not make every
Cover dark.

Target a strict 350:155 landscape composition. Generate masters at 1400×620 or
700×310. Draw no external card frame; Launcher supplies it. Evaluate visual
complexity on the physical 1-bit target, never from the enlarged master alone.

These constraints are derived from *Designing for Playdate*, SDK 1.12.3: the
400×240 reflective 1-bit screen depends on area, contour, spacing, and deliberate
texture; important strokes should be at least 2 pixels; automated dithering can
become fuzzy; and high-frequency patterns can flash when shifted by one pixel.

## Visual complexity budget

Before composing a prompt, write a permitted-element list containing only:

1. the exact App title;
2. one dominant App-defining object;
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

### Shared style suffix

Append this text to every App prompt:

```text
retro-futurist portable instrument interface, 1980s scientific field manual, Swiss editorial composition, bold geometric display typography with smooth high-resolution contours, strong app title integrated into the artwork, one dominant object, one broad supporting shape, large readable silhouettes, radical visual economy, generous quiet negative space, playful but precise, high-contrast composition designed for later 1-bit ordered-dither conversion, black white and three to five deliberate gray values used only as large planar surfaces, restrained planar shading rather than photographic gradients, clean antialiased master edges, use only the explicitly permitted elements and invent no additional props, no card border, no rounded container, landscape 350:155 aspect ratio, title fully inside safe margins
```

### Negative prompt

Apply these as explicit exclusions:

```text
Exclude: photorealistic, glossy UI, cyberpunk neon, colorful, soft drop shadows, continuous photographic gradients, blur, intentionally jagged pixel-font stair-steps in the master, thin lines, tiny text, dense dashboard, generic mobile app UI, multiple panels, external frame, rounded card border, miniature architecture, cables, bolts, screws, railings, antennas, star fields, skyline clutter, debris, decorative particles, extra icons, ornamental instruments, speckle, random texture, invented background props, Playdate logo, copyrighted characters.
```

## Typography and grayscale

- Design title glyphs as smooth high-resolution shapes first. Pixel hinting and
  1-bit cleanup happen after reduction; do not confuse “bitmap-inspired” with
  deliberately rough diagonal and curved outlines.
- Keep final important strokes at least 2 pixels thick. Use generous cap height,
  clear counters, distinguishable letters, and manual cleanup after conversion.
- Use gray as a small material vocabulary, not generic noise: highlight face,
  side plane, metal, atmosphere, or depth. A strong title treatment is a
  dithered/light upper face over a solid black lower face or extrusion.
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

### CLOCK

Purpose: stopwatch/timer; rotary input adjusts time and Confirm pauses/resumes.
Art role: the set's quiet, exact and slightly mysterious night scene.

```text
A radically simplified launcher cover for an app titled “CLOCK”. Use only the word “CLOCK”, one large circular clock dial with two bold hands, one simple geometric clock-tower silhouette, one sweeping triangular beam, and optionally one compact digital readout “12:34”. The dial, beam, and title must carry the composition. Keep the tower as a bold mass with at most two large internal planes, not a rendered building. Do not add a moon, stars, distant lighthouse, skyline, railings, ladders, antennas, windows, rocks, vegetation, or ground debris. The scene should feel quiet, exact, nocturnal and slightly mysterious. Spell the title exactly “CLOCK”.
```

Keep the displayed time static in all Launcher interactions.

### MOTION

Purpose: spring/inertia experiment; rotary input changes the target and a circle
tracks it elastically. Art role: the bright kinetic laboratory.

```text
A radically simplified bright kinetic scene for an app titled “MOTION”. Use only the word “MOTION”, one large hollow circular test object, one horizontal track, one target marker, and at most three broad speed marks. The circle and direction of travel must carry the composition. Do not combine a grid, particles, multiple arrow systems, measurement ticks, or secondary instruments. Use a mostly white background with bold black geometry and generous quiet space. The focal object must remain hollow white inside, never a large solid black ball. Spell the title exactly “MOTION”.
```

### SETTINGS

Purpose: control center for motion intensity, sound, Wi-Fi/provisioning and
Launcher orientation. Art role: a calm, authoritative, mostly white precision
console—not a settings list.

```text
A radically simplified precision console for an app titled “SETTINGS”. Use only the word “SETTINGS” and three large tactile control silhouettes on one common base plane: a rotary dial, a physical toggle, and a horizontal slider. Give each control one bold state-defining mark and no ornamental hardware. Use a mostly white background, orderly spacing, and generous quiet space. Do not add connecting paths, Wi-Fi symbols, screws, labels, calibration ticks, extra panels, or a conventional settings list. Spell the title exactly “SETTINGS”.
```

### ANIMATION GALLERY

Purpose: demonstrate easing, spring, transition, particles, camera, sprite state
and dither. Art role: the dark but active experimental stage.

```text
A radically simplified animation stage for an app titled “ANIMATION GALLERY”. Use only the stacked title “ANIMATION GALLERY”, one core geometric figure shown in exactly three successive large poses, and one continuous easing arc or spring path. The repeated figure and single path must express sequence and transformation. Use a theatrical black background with crisp white silhouettes and generous empty space. Do not add particles, frame numbers, chart axes, multiple curves, camera marks, secondary icons, or unrelated function diagrams. Spell the title exactly “ANIMATION GALLERY”.
```

## Acceptance checklist

Reject or iterate unless every applicable item passes:

- Canvas composition is exactly 350:155 or safely crop-ready to that ratio.
- Exact App title is legible, fully inside safe margins, and carries 20–30% of
  visual weight.
- The Cover contains one primary scene and one dominant geometric object.
- The Cover stays within three semantic layers: title, dominant object, and one
  broad supporting shape; there are no more than two small semantic accents.
- CLOCK/GALLERY read as dark and MOTION/SETTINGS read as light.
- Typography, margins, contour weight and editorial rhythm form one family.
- No external border, rounded card, page indicator, input hint or Launcher chrome.
- Master contours are smooth and antialiased; final 1-bit contours are manually
  hinted with no isolated stair-step noise.
- Gray planes add intentional hierarchy or volume and survive as controlled
  ordered dither; there are no broad fuzzy auto-dither fields.
- Important final strokes are at least 2 pixels thick and counters remain open
  at both 350×155 and 280×124.
- No tiny labels or dense dashboard structures.
- No decorative micro-architecture, star field, skyline clutter, debris, repeated
  hardware, or texture used merely to fill space.
- Important contours remain readable after a 4× reduction to 350×155.
- Every retained detail remains meaningful and stable at both 350×155 and
  280×124 when viewed at 1×; enlarged-master appeal is not an acceptance reason.
- MOTION's core circle is hollow; CLOCK has one main dial.
- Image is a static identity Cover, not a pressed/selected/launch variant.
- Original lossless PNG is retained separately from derived crops and 1-bit files.
- Pure black/white and reflective-palette previews are reviewed at 1× size.
- A short moving-track review shows no objectionable dither shimmer or flashing.
