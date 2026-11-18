# CRUMBS

A drum mixer insert for FL Studio. Put it on a drum bus or a single drum channel, the same way you would put on a compressor. It shapes the drums already running through that track. You do not drop a loop in.

Two modes, always your choice:

- **Whole** (default) — treat the drum sound as one thing. Punch, body, snap, crunch, tone, glue, room, width, mix and output.
- **Split** — a live frequency split into Kick, Snare, Hats and Perc. Shape each part on its own, or solo it with Listen. This is not a stem separator: parts will bleed. Use Listen to hear what each orb is catching.

The drum bus starts on **Clean**, which is a true bypass. Tape Warm, Blown Speaker, Smashed Room and Bedroom Crunch are punchy, saturated indie-drum colours. A/B compares the processed sound to the dry input.

v2.0.1 is a macOS universal VST3 and AU. Nothing here is proven on real drums in FL until you try it, and it is not installed on your Mac until you ask.

## Latency and loudness

Default latency is **0 samples**. An optional Lookahead safety (about 1.5 ms) is off by default; when you turn it on, the plugin reports that delay to the host.

Processors that add gain use auto gain compensation so a preset is a character, not a jump in level. A clip-safe ceiling (default −0.1 dBFS) sits at the end of the chain. Clean does not run the ceiling: a legal input comes out bit-exact.

## Building

See [BUILD.md](BUILD.md). Tests are in [TESTING.md](TESTING.md). The design is in [PLAN.md](PLAN.md).

Engine tests have no JUCE dependency. The plugin is JUCE 8 / CMake, VST3 and AU (VST3 only on Linux).
