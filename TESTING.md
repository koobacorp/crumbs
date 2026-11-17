# Testing CRUMBS

Automated tests run on the engine (no window needed) and again in the plugin, the same way as BAKERY.

## Engine

`crumbs_engine_tests` covers:

- Onset times and labels on synthetic loops with known hits
- Isolated-hit labelling (target ≥ 95%) and a busier loop (target ≥ 85%)
- Overlapping hits staying in one slice
- Lane add / split / merge / rename / recolour / ninth-lane rejected
- Arrange, duplicate-to-extend (2→4→8 and six pieces from four), delete, nudge, copy, undo
- Clean bus as a true bypass; saturation curves; pan laws; pitch shift of a known interval
- Rebuild / Blend / A/B
- Keep in key and concert-pitch note display (settled body pitch)
- MIDI export header; playback safety limiter

## Plugin

`crumbs_plugin_tests` covers parameters, loading a synthetic loop, every automatable control, Clean and the four bus presets, A/B, Keep in key, Rebuild and Blend, undo, audio and MIDI save, sampler MIDI, state round-trip, tempo follow, and UI screenshots:

- `CRUMBS-ui-1-empty-drop.png` — empty drop state
- `CRUMBS-ui-2-loaded-slices.png` — loaded loop with coloured slices
- `CRUMBS-ui-3-lane-orb.png` — a selected lane and its orb
- `CRUMBS-ui-4-bus-clean.png` — drum bus with Clean selected
- `CRUMBS-ui-5-rebuild.png` — rebuild / extra lane

pluginval at strictness 10 runs on the Mac build, if CI is allowed to run on the private repo.

Listening in FL is still the real test. Automated tests cannot tell you if Tape Warm feels good on your loops.
