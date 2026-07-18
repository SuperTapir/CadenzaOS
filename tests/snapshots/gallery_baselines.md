# Animation Gallery visual baselines

These hashes capture paused 50% progress after direct knob scrubbing. They
were accepted after inspecting PBM-to-PNG renders at both supported profiles
on 2026-07-18. The selected pages cover the easing graph, a dark-field
transition, a coverage transition, particles, atlas/state animation, and
ordered dither. `cadenza_gallery_snapshot_tests` owns the executable values.

| Profile | Page | Hash (decimal) |
| --- | --- | ---: |
| 320×170 | Easing | 11440640267301194983 |
| 320×170 | Dip | 4137199271609631912 |
| 320×170 | Checker dissolve | 9878436293935264858 |
| 320×170 | Particles | 11948840630181533057 |
| 320×170 | Sprite state | 9026563825525625221 |
| 320×170 | Dither | 13029273646656056373 |
| 400×240 | Easing | 17615136595640686815 |
| 400×240 | Dip | 15534794435185275820 |
| 400×240 | Checker dissolve | 13438891652020997183 |
| 400×240 | Particles | 7317937573070898047 |
| 400×240 | Sprite state | 1640077496920006473 |
| 400×240 | Dither | 17314096927354342150 |

Accepted trade-offs: Gallery favors deterministic, inspectable scenes over
asset richness; particle scrubbing does not reconstruct particle history;
reduced motion scales feedback and camera amplitudes but preserves direct
Tween, transition, Timeline, and frame-animation scrubbing. All comfortable
defaults live in `cadenza/presentation/defaults.h`.
