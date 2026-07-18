# CadenzaOS Launcher Cover art direction

## Shared system

Unify visible Launcher Apps as one “retro-futurist experimental instrument /
scientific field manual” product family: strong titles, one core scene, broad
black/white relationships, and sparse purposeful details. Do not make every
Cover dark.

Target a strict 350:155 landscape composition. Generate masters at 1400×620 or
700×310. Draw no external card frame; Launcher supplies it.

### Shared style suffix

Append this text to every App prompt:

```text
retro-futurist portable instrument interface, 1980s scientific field manual, Swiss editorial composition, bold geometric display typography with smooth high-resolution contours, strong app title integrated into the artwork, large readable silhouettes, sparse intentional details, playful but precise, high-contrast composition designed for later 1-bit ordered-dither conversion, black white and three to five deliberate gray values, restrained planar shading rather than photographic gradients, clean antialiased master edges, no card border, no rounded container, landscape 350:155 aspect ratio, title fully inside safe margins
```

### Negative prompt

Apply these as explicit exclusions:

```text
Exclude: photorealistic, glossy UI, cyberpunk neon, colorful, soft drop shadows, continuous photographic gradients, blur, intentionally jagged pixel-font stair-steps in the master, thin lines, tiny text, dense dashboard, generic mobile app UI, multiple panels, external frame, rounded card border, Playdate logo, copyrighted characters.
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
- Because the whole Cover moves in Launcher, inspect gray patterns during
  one-pixel animation. Reduce or coarsen patterns that shimmer or flash.

## App briefs

### CLOCK

Purpose: stopwatch/timer; rotary input adjusts time and Confirm pauses/resumes.
Art role: the set's quiet, exact and slightly mysterious night scene.

```text
A cinematic launcher cover for an app titled “CLOCK”. A solitary mechanical clock tower or precision chronometer stands on one side of a dark night landscape. Its circular dial is clearly visible, with two bold hands and a sweeping beam of light cutting across the composition toward the large word “CLOCK”. Add a small secondary digital time readout such as “12:34”. The scene should feel quiet, exact, nocturnal and slightly mysterious. Use one dominant circular shape, a strong light beam, sparse horizon marks and a large highly readable title. Show exactly one main clock dial, not a collection of clocks. Spell the title exactly “CLOCK”.
```

Keep the displayed time static in all Launcher interactions.

### MOTION

Purpose: spring/inertia experiment; rotary input changes the target and a circle
tracks it elastically. Art role: the bright kinetic laboratory.

```text
A bright kinetic laboratory scene for an app titled “MOTION”. A single hollow circular test object travels along a precise horizontal experimental track toward a small target marker. Long speed lines and a restrained measurement grid communicate spring motion, inertia and overshoot. The large word “MOTION” is part of the scene, not a separate UI label. Use a mostly white background with bold black geometry, one hollow circle as the focal point, a clear direction of travel and generous negative space. Energetic, technical and playful. The focal object must remain hollow white inside, never a large solid black ball. Spell the title exactly “MOTION”.
```

### SETTINGS

Purpose: control center for motion intensity, sound, Wi-Fi/provisioning and
Launcher orientation. Art role: a calm, authoritative, mostly white precision
console—not a settings list.

```text
A precision control console for an app titled “SETTINGS”. Show an elegant modular machine with three large tactile controls: a rotary dial, a physical toggle switch and a horizontal calibration slider. Thin connecting paths suggest sound, motion, wireless connectivity and launcher orientation without using tiny labels. The word “SETTINGS” is large and unmistakable, integrated into the industrial panel composition. Mostly white background, bold black controls, orderly spacing, calm and authoritative, like a beautifully designed scientific instrument rather than a phone settings menu. Do not include tiny Wi-Fi labels or a conventional list UI. Spell the title exactly “SETTINGS”.
```

### ANIMATION GALLERY

Purpose: demonstrate easing, spring, transition, particles, camera, sprite state
and dither. Art role: the dark but active experimental stage.

```text
An experimental animation stage for an app titled “ANIMATION GALLERY”. A simple geometric figure appears in four successive motion poses across the scene, connected by elegant easing arcs and a spring curve. A small burst of square particles and staggered frame marks suggest transitions, camera motion and sprite animation. The title “ANIMATION GALLERY” is large, stacked in two strong lines and integrated into the composition. Use a theatrical black background with crisp white motion paths and a few bold geometric shapes. Express sequence, timing and transformation without looking like a chart or software dashboard. Show four successive poses of one core figure rather than four unrelated function charts. Spell the title exactly “ANIMATION GALLERY”.
```

## Acceptance checklist

Reject or iterate unless every applicable item passes:

- Canvas composition is exactly 350:155 or safely crop-ready to that ratio.
- Exact App title is legible, fully inside safe margins, and carries 20–30% of
  visual weight.
- The Cover contains one primary scene and one dominant geometric object.
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
- Important contours remain readable after a 4× reduction to 350×155.
- MOTION's core circle is hollow; CLOCK has one main dial.
- Image is a static identity Cover, not a pressed/selected/launch variant.
- Original lossless PNG is retained separately from derived crops and 1-bit files.
- Pure black/white and reflective-palette previews are reviewed at 1× size.
- A short moving-track review shows no objectionable dither shimmer or flashing.
