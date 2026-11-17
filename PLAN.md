# CRUMBS — a drum-loop plugin for FL Studio

Status: **plan approved, building v1.0.** Private repo (koobacorp/crumbs). macOS universal VST3, same way as BAKERY and MergeX.
Nothing here is proven on real loops in FL until you try it. Not installed on your Mac until you ask.

**CRUMBS** is a plugin you drop a drum loop into. It cuts the loop into colour-coded pieces, sorts each hit into a lane, and lets you tweak, move, copy and extend those pieces. You can optionally rebuild a part as a synth sound, lock pitched drums to a key, and — only if you want it — run the whole thing through punchy, saturated indie-drum bus presets. The drum bus starts on **Clean**, so rearranging and editing is dry until you pick a coloured preset.

It is not a rewrite of BAKERY. BAKERY makes drums from scratch. CRUMBS takes drums you already have and lets you pull them apart.

Left out of this version, on purpose: a step sequencer, and loading more than one loop at a time. Those would clutter the simple layout.

Splitting is **free and local**. There is no paid cloud GPU, no rented training, and no giant neural un-mixer in v1.0. Overlapping hits that land in the same cut stay together; Fix-the-sort, split, merge and nudge cover the rest.

---

## 1. What you do with it

1. Drop a loop onto the plugin (WAV, AIFF, FLAC, or a clip dragged from FL).
2. It finds every hit and paints the loop across the top, cut into coloured sections, so you can start work straight away.
3. Click a colour (or a single slice) and tweak that part: level, pitch, decay, crunch, room, and so on.
4. Rearrange freely: drag slices to new places, copy or duplicate sections to make a longer loop (2 bars into 4 or 8, or six pieces instead of four), delete or reorder sections. All of this has undo.
5. If a hit landed in the wrong lane, tap it and move it (Fix-the-sort). You can also split, merge, add, rename and recolour lanes (up to eight).
6. Optionally press **Rebuild** to make a synth version of that lane, with a few big knobs and a Blend slider.
7. Play it as a tempo-synced loop, or as a sampler (one MIDI note per lane).
8. Drag a lane, or the whole edited loop, out to FL as audio or MIDI.
9. The drum bus starts on **Clean** (no extra effect). Pick a bus preset only when you want that crushed, excited indie sound.

---

## 2. How it splits the loop

There is one fast, free path, always on.

### Onset slicing

It listens for each hit (a sudden rise in energy / “onset”), cuts the loop at those points, and labels each slice into a starting lane: Kick, Snare/Clap, Hats, or Perc. This is instant and runs on the computer, with no network and no paid compute.

### Smart per-hit classification

Each slice is measured for simple spectral features, in plain language:

- **Thump** — how much low-end body (a kick)
- **Snap** — a mid-range crack at the start (a snare)
- **Sizzle** — bright, short high-frequency energy (hats)
- plus brightness (spectral centroid) and how long the hit rings

A rule-based sort uses those features first. On top of that, a **small optional classifier** can be trained on this machine’s CPU from licence-clean data only: hits we synthesise ourselves (including with BAKERY’s engines) and, if a modest licence-clean set is practical to keep on the box, a subset of StemGMD (CC BY 4.0). We will not download a hundreds-of-gigabytes dataset, and we will not rent GPUs. The classifier, if used, is tiny (a linear or shallow model, kilobytes of weights), trained once on the box and baked into the plugin.

### Honest limit: overlapping hits

If a kick and a hat land at the same moment, that slice still holds both. Editing the kick will nudge the hat too. There is no “Deep split” neural un-mixer in this version — that would have needed paid GPU training we are not doing.

What covers the rest:

- **Fix-the-sort** — tap a slice and send it to any lane
- **Split / merge lanes** — for example Hats into Open and Closed, or Kick into Kick and 808, then sort slices by hand
- **Nudge** — move a hit earlier or later

---

## 3. The lanes

You start with four colour-coded lanes, and you can grow, split, merge and rename them, up to **eight lanes** in total.

Default starting set:

- **Kick** — warm orange-red
- **Snare / Clap** — violet-blue
- **Hats** — green
- **Perc** (toms, rims, and anything else) — strong golden yellow, so it does not disappear on the beige background

Each colour is one solid hue. No rainbow blends inside a lane. The loop across the top is cut into those coloured sections.

### Flexible lanes (up to 8)

- **Add a lane**, up to eight, when you need another part (for example a separate 808, a clap lane, or a shaker).
- **Split a lane into two.** Typical cases: Hats → Open Hats and Closed Hats; Kick → Kick and 808. This is a lane split plus a best-effort sort of the slices; you can Fix-the-sort anything it gets wrong.
- **Merge lanes** back together (for example Clap back into Snare).
- **Rename** any lane.
- **Recolour** any lane from a simple fixed palette of single solid hues. No blends, no custom colour picker. The palette is chosen to stay distinct on the beige background, for example: warm orange-red, violet-blue, green, golden yellow, teal, magenta, rust, and steel blue.
- **Fix-the-sort works across all lanes**, including ones you added or renamed: tap a slice and send it to any lane.

Sampler mode gives each lane, including extra ones, its own MIDI note. Rebuild, Blend and the per-lane knobs apply to whichever lane is selected.

### What each lane can do

- Level, mute
- Pitch, and **tune-to-key** (see Key lock)
- Decay (shorten or lengthen the tail)
- Tone (darker ↔ brighter)
- Crunch (saturation on that lane only)
- Pan
- Room / space (so a snare can sit in a room while the kick stays dry)
- Humanize and swing (per lane, so hats can shuffle while the kick stays tight)
- Randomize (happy accidents, one lane at a time)
- Ghost hits (add or thin out the quiet in-between snare and hat hits)
- Nudge a single hit earlier or later
- Fix-the-sort: tap a slice and move it to any other lane

---

## 4. Rearranging the loop

The loop is not a fixed strip. You can treat the coloured sections as pieces you move around, all with undo.

- **Drag a slice** to a new position on the timeline.
- **Copy or duplicate** a slice or a whole section, including to **extend the loop**. For example, turn a 2-bar loop into 4 or 8 bars, or build six pieces out of four, by repeating or rearranging cuts. The new length becomes the loop length; playback still follows FL’s tempo.
- **Delete** a slice or section.
- **Reorder** sections (swap, move a bar later, and so on).

A/B still compares against the original dropped loop, so you can hear whether the new arrangement actually helps. Undo steps back through edits and arrangement changes.

This stays a simple arrange strip, not a second playlist. No extra loops loaded at once, and no step sequencer.

---

## 5. Rebuild (synth version of a lane)

Each lane has a **Rebuild** option. It listens to the original hits in that lane and builds a synth version using BAKERY’s drum engines (the same kick, snare, and hat synthesis, reused, not rewritten from scratch).

This will not be a perfect clone of the sample. That is why every lane also has a **Blend** slider:

- All the way left: the original sample
- All the way right: the rebuilt synth sound
- In the middle: both layered, which is usually the sweet spot — you keep the loop’s vibe and gain control

Once rebuilt, the lane shows only a few big, obvious knobs (not BAKERY’s full panel):

- **Kick:** Punch, Boom (length), Tune, Crunch
- **Snare / Clap:** Body, Snap, Tune, Crunch
- **Hats:** Tone, Length, Shimmer, Crunch
- **Perc** (and other lanes): Body, Tune, Length, Crunch

Under the hood, Rebuild looks at the original hit’s settled pitch, how long it rings, how bright the start is, and how dirty it is, then sets the BAKERY engine to match as close as it reasonably can. The big knobs stay in charge after that.

---

## 6. Key lock

Kicks and toms (the drums that actually have a note) get their pitch detected and shown as a plain note name plus cents, for example **E1 +8 cents**. The same applies to an 808 lane if you have split one off.

You pick a key and scale (say E minor). **Keep in key** then snaps those pitched drums to the nearest note in the scale, or, if you choose, to the root of the key. Hats and most percussion are left alone — they usually have no useful musical pitch.

When a kick has a pitch drop (the classic 808 “hit then boom”), the note we show and snap is the **settled body pitch** after the drop, not the click at the start. That is the note your song actually sits on.

---

## 7. Playing it, comparing, and dragging out

- **Loop mode:** plays in time with FL’s tempo and transport, so the loop follows the song. If you have extended the loop, it plays the new length.
- **Sampler mode:** each lane gets its own MIDI note, so you can rewrite the pattern from the piano roll or a controller.
- **A/B:** hold or toggle to hear the untouched original loop against your edit and arrangement.
- **Undo** for lane edits, Fix-the-sort, and rearranging.
- **Drag out:** drag one lane, or the whole edited loop, into FL as audio. You can also drag MIDI (one note per lane, hits as notes) onto the playlist or piano roll. If FL ignores the drag, there is a Save fallback, the same idea as BAKERY’s MIDI export.

---

## 8. The drum bus (and the Clean default)

This sits on the whole loop, after the lanes. It is the “make these drums punchy, saturated, and excited” section, with these controls:

- **Punch** — brings out the start of the kicks and snares so they still cut after the dirt
- **Crunch** — with a type: Tape, Tube, Clip, or Fuzz
- **Squash** — a heavily compressed copy blended under the clean drums (parallel compression)
- **Pump** — the kick gently ducks everything else
- **Room** — a space around the kit

**Clean is the default.** It is a true bypass of the bus: no punch, no crunch, no squash, no pump, no room. Drop a loop in, rearrange it, edit lanes, and drag it out, and you will hear the drums themselves — not a distorted preset. Clean is also a preset you can click back to at any time.

### How that modern indie drum sound is actually made

It is rarely one plugin. The records this is aiming at (Fred again.., Dijon, Mk.gee, later Bon Iver, that bedroom-indie crunch) usually stack a few simple moves:

1. **Shape the hit first.** A little transient punch so the kick and snare still knock after you smash them.
2. **A clean-ish drum bus**, often with gentle tape or console-style saturation, then a glue compressor. On Bon Iver’s *22, A Million*, the drum bus was a saturator followed by a warm compressor.
3. **A parallel “crush” bus.** Send a copy of the drums into heavy compression (hard ratio, lots of gain reduction) plus more saturation, then blend that thick, loud copy *under* the clean drums. You add body and excitement without flattening the original attack. This is the usual FINNEAS / Fred again..-style trick.
4. **Push it on purpose.** Mk.gee has described recording drums lightly, then “just saturation… push it hard after.” The kick can go slightly flabby, the snare a bit echo-y. That is part of the feel, not a mistake.
5. **A little room or lo-fi bandwidth**, so it feels recorded rather than programmed. Pumping from the kick is optional, but it is a big part of the excited, breathing feel.

### Presets

- **Clean** — default. Bus bypassed. For editing and rearranging with no extra colour.
- **Tape Warm** — gentle tape crunch, light glue, a little punch, soft room. The “expensive cassette” version.
- **Blown Speaker** — harder clip or fuzz, slightly narrower bandwidth, more crunch, less pretty room. Cheap mic, slightly broken speaker.
- **Smashed Room** — heavy parallel squash, bigger room, more pump. The drums swell between hits.
- **Bedroom Crunch** — mid-gain tape plus a bit of clip, short boxy room, modest pump. Close to that Dijon / Mk.gee “play light, push hard” feel.

The coloured presets are starting points you opt into. They cannot promise to sound like a specific record, and they will not be verified until you put them on your loops.

---

## 9. Look and feel

The look is taken from a set of minimal record-sleeve posters: each sleeve is almost empty, with one soft, blurry, glowing orb in the middle and tiny thin grey uppercase type tucked into the corners. We keep that mood, with two changes you asked for.

- **Background:** warm beige, not white.
- **Almost everything is neutral:** beige, warm greys, soft charcoal text. The only strong colour on the screen is the lane hues.
- **Colour coding stays simple.** One solid hue per lane. The reference orbs are multi-coloured, like a swirl of several hues in one sphere. We do not do that inside a lane. Each orb, slice, and lane glow is a single colour, just softly blurred and luminous. When you recolour a lane, you pick from the fixed solid palette, never a blend.
- **Soft glow and blur.** Lane strips and selected slices glow gently in their colour. When a lane is selected, a soft glowing orb in that lane’s colour sits behind its big knobs, like the sleeve posters, only warmer and single-hued.
- **Type:** tiny, thin, uppercase labels at the edges (the catalogue-style corner text), plus larger, high-contrast values on the main knobs so the important controls are obvious on the beige.
- **Window:** resizable, with a sharp layout that scales cleanly. High contrast on the controls you actually grab: knob values, the selected slice, Play, A/B, Blend, and the arrange edits.

Reference image (for us while building, not shipped inside the plugin): `refs/ui-reference-orbs.jpg`.

### Layout (described)

```
  CRUMBS                                    KEY  E MIN    KEEP IN KEY     A/B   UNDO

  [======= KICK =======][ SNARE ][======== HATS ========][PERC][ KICK ] ...
   orange-red             violet-blue   green              yellow
   (drag, copy, delete, or duplicate slices here to rearrange or extend)

  [ KICK ]   [ SNARE ]   [ HATS ]   [ PERC ]  [ + ]     PLAY:  Loop  |  Sampler
   selected     .           .          .     add lane    DRAG OUT

            ( soft orange-red orb behind the knobs )

              PUNCH        BOOM         TUNE        CRUNCH         BLEND
               (O)          (O)          (O)          (O)        orig ---|---- rebuilt

  level   pitch   decay   tone   crunch   pan   room   swing   humanize   ghosts

  DRUM BUS    Clean*  |  Tape Warm  |  Blown Speaker  |  Smashed Room  |  Bedroom Crunch
  punch     crunch (tape/tube/clip/fuzz)     squash     pump     room
              * Clean is the default: bus off
```

Top: the loop, cut into coloured sections. Click a section to select its lane; drag a slice to move or nudge it; copy or duplicate to extend the loop; use a small menu on the slice to move it to another lane, delete it, or split from here.

Middle: lane buttons (plus an add-lane control), then the selected lane’s Rebuild knobs and Blend, with that lane’s soft orb behind them.

Lower: the finer per-lane controls, then the drum bus along the bottom, with **Clean** selected until you pick something else.

Empty state, before you drop a loop: a quiet beige panel with a single soft, neutral orb and a small uppercase line like DROP A LOOP — the same poster feeling, with almost nothing on it.

---

## 10. How it is tested

Automated tests run on the engine (no plugin window needed) and again in the plugin, the same way as BAKERY and MergeX. pluginval at strictness 10 on the Mac build, if CI can run on the private repo.

- **Hit detection and labelling:** we build synthetic loops where we already know every hit’s time and type. Slicing must find those times within a tight window. Labelling targets: **≥ 95%** correct into the default four lanes on clean isolated hits, and **≥ 85%** on busier loops (including some close-together hits), with precision and recall per class. Overlapping hits that share one onset are expected to stay in one slice.
- **DSP against textbook math:** pitch shift by a known number of semitones; decay times; pan laws; note-from-frequency (A4 = 440 Hz) and snap-to-scale; saturation curves for tape / tube / clip / fuzz; compressor gain reduction on a known signal; tempo-sync of the loop against a host tempo.
- **Lanes:** add up to eight, split (including hats → open/closed and kick → kick/808), merge, rename, recolour from the palette only, and Fix-the-sort across every lane. A ninth lane is rejected.
- **Rearranging:** drag, copy, duplicate-to-extend (2 bars to 4 and to 8, and building six pieces from four), delete, and reorder, each with undo back to the previous state. Extended loops report the new length and still follow host tempo.
- **Rebuild:** a BAKERY-made kick (or snare, or hat) is dropped in; Rebuild should land in the right engine and come out in a sensible range for pitch, length, and brightness. Blend at 0 matches the original slice; Blend at 100 is the synth only.
- **Key lock:** known pitches snap to the expected scale notes; a kick with a pitch sweep still reports and snaps the settled body note, not the start click.
- **Drum bus:** a new loop starts on Clean; with Clean selected, the bus output matches the pre-bus signal. The other presets change the sound in the expected direction (more crunch, more squash, and so on) and can be returned to Clean.
- **Undo and A/B:** an edit or arrangement change, then Undo, returns the previous state; A/B outputs the untouched original.
- **MIDI and audio export:** dragged MIDI has one note per lane (including extra lanes) at the right times; audio export length and lane isolation are checked.
- **UI screenshots** of the empty drop state, a loaded loop with coloured slices, a selected lane with its orb, and the drum bus with Clean selected.
- **Safety:** no crashes, no denormals, no audio-thread allocations, output does not explode on hot loops (a limiter on the way out).
- **Every control** is exercised in the plugin tests.

Listening in FL is still the real test. Automated tests cannot tell us if Tape Warm feels good on your loops.

---

## 11. Risks and limits (the honest part)

- Auto-sorting will get some hits wrong, especially on busy, lo-fi, or already-distorted indie loops. Fix-the-sort is not a nice extra — it is required.
- Overlapping drums that land in the same cut cannot be un-baked. There is no neural Deep split in this version.
- A classifier trained only on clean synthesised hits may stumble on crushed, lo-fi, or unusual loops. We will report the measured accuracy honestly.
- Rebuild is an interpretation, not a clone. Blend exists because of that.
- Pitch detection is reliable on 808-style kicks and toms with a clear note, and shaky on short clicky kicks. When we are unsure, we show that rather than pretend.
- Drag-and-drop into FL can be flaky, which we already know from BAKERY. There will be a Save fallback.
- Rearranging is kept simple on purpose. It will not replace FL’s playlist.
- Private-repo GitHub Actions has been blocked by billing before. If CI is still blocked, the Mac zip will not be produced until that is resolved, without making the repo public.
- None of this is verified in FL Studio until you open it and put it on your music.

---

## 12. Rough size and build

- The v1.0 Mac zip should stay in the same range as the other instruments, on the order of **5–15 MB**, because there is no large neural model to bundle.
- Playback CPU should be modest (it is a sampler plus optional bus processing). Slicing and classification run once when you drop a loop, not on every audio buffer.
- macOS universal VST3, built on GitHub Actions if the private repo is allowed to run workflows, plus engine tests, plugin tests and pluginval. Training, if any, is only on this box’s CPU with licence-clean data. Not installed on your Mac until you ask.
