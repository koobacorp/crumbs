# CRUMBS

A drum-loop plugin for FL Studio. You drop a loop in; it cuts the loop into colour-coded pieces, sorts each hit into a lane, and lets you tweak, move, copy and extend those pieces. You can optionally rebuild a lane as a synth sound, lock pitched drums to a key, and run the whole thing through punchy indie-drum bus presets.

The drum bus starts on **Clean** (a true bypass). Rearranging and editing stays dry until you pick a coloured preset.

CRUMBS is not a rewrite of BAKERY. BAKERY makes drums from scratch. CRUMBS takes drums you already have and lets you pull them apart.

v1.0 is a macOS universal VST3. The repo is private. Nothing here is proven on real loops in FL until you try it, and it is not installed on your Mac until you ask.

## What you do with it

1. Drop a WAV, AIFF or FLAC loop onto the plugin (or a clip dragged from FL).
2. It finds each hit and paints the loop across the top, cut into coloured sections.
3. Click a colour (or a single slice) and tweak that part: level, pitch, decay, crunch, room, swing, and so on.
4. Rearrange: drag slices, copy or duplicate to make a longer loop (2 bars into 4 or 8, or six pieces instead of four), delete or reorder. All of this has undo.
5. If a hit landed in the wrong lane, right-click it and send it to another lane (Fix-the-sort). You can also split, merge, add, rename and recolour lanes, up to eight.
6. Optionally press **Rebuild** for a synth version of that lane, with a few big knobs and a Blend slider.
7. Play it as a tempo-synced loop, or as a sampler (one MIDI note per lane).
8. Drag a lane, or the whole edited loop, out to FL as audio or MIDI. If FL ignores the drag, use **Save audio** / **Save MIDI**.

## How it splits the loop

Splitting is free and local. There is no paid cloud GPU and no neural un-mixer in v1.0.

It listens for a sudden rise in energy (an onset), cuts there, and labels each slice using simple features: **thump**, **snap**, **sizzle**, brightness and how long the hit rings. A rule-based sort uses those first.

If a kick and a hat land at the same moment, that slice still holds both. Fix-the-sort, split, merge and nudge cover the rest.

Note names are **concert pitch** (A4 = 440 Hz, C4 = MIDI 60). FL Studio’s piano roll often labels that C5. The small “concert pitch” note on the display is there so the two are not confused.

**Keep in key** snaps kicks and other pitched lanes to the settled body pitch after any click or drop, not the start of the sweep. Hats are left alone.

## The drum bus

- **Clean** — default, true bypass
- **Tape Warm**
- **Blown Speaker**
- **Smashed Room**
- **Bedroom Crunch**

Each preset is punch, crunch (tape / tube / clip / fuzz), parallel squash, pump (kick ducking) and room, in different amounts. You can still move those knobs after picking a preset.

## Building

See [BUILD.md](BUILD.md). Tests are in [TESTING.md](TESTING.md). The full plan is in [PLAN.md](PLAN.md).

Engine tests have no JUCE dependency. The plugin is JUCE 8 / CMake, VST3 only.
