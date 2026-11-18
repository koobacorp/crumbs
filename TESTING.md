# Testing CRUMBS v2

Automated tests run on the engine (no window needed) and again in the plugin.

## Engine

`crumbs_engine_tests` covers:

- Saturation, pan/width identity, and the clip-safe ceiling
- Clean as a bit-exact bypass at 44.1 kHz and 48 kHz, including a 0 dBFS sample
- Split mode unity reconstruction (default and moved crossovers)
- Latency vs impulse: 0 samples on the default path (including crunch), and lookahead delay equal to `latencySamples()`
- No output above the ceiling on stress signals (Smashed Room maxed, Whole max, Split max, several block sizes, +6 dB input)
- Bus and Whole starting points stay under the ceiling, return to a Clean null, and are gain-compensated
- Split Listen / Mute sanity on a synthetic kick / snare / hat / perc kit
- Each control moves the audio; Mix and Output extremes
- CPU budget: a 512-sample block of Split + Smashed + crunch well under 1 ms, and a 2 s 48 kHz buffer well under real time

## Plugin

`crumbs_plugin_tests` covers the stereo insert layout (not an instrument), every automatable control, Clean and A/B nulls, latency vs impulse (including host bypass with lookahead on), ceiling stress, bus presets, state round-trip, CPU, and UI screenshots:

- `v2-whole-clean.png`
- `v2-whole-fatbody.png`
- `v2-split-orbs.png`
- `v2-split-listen-kick.png`
- `v2-bus-smashed.png`

pluginval at strictness 10 runs on the Mac VST3 and AU. auval runs on the AU.

Listening in FL is still the real test. Automated tests cannot tell you if Tape Warm feels good on your drums.
