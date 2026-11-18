# CRUMBS v2 — a drum mixer insert

Status: **v2.0.** Stereo audio effect (not an instrument). macOS universal VST3 + AU. Public repo [koobacorp/crumbs](https://github.com/koobacorp/crumbs).

Put CRUMBS on an FL mixer track. It processes the drums already running through that track in real time. Two modes: **Whole** (default) shapes the kit as one sound; **Split** is a live band split into Kick, Snare, Hats and Perc. The drum bus starts on **Clean** (a bit-exact bypass). No loop loading, no rearrange, no rebuild, no MIDI, no ML, no paid compute.

## Signal path

```
input
  ├─ A/B or full bypass ──────────────────────── bit-exact out
  ├─ Whole: punch, body, snap, tone, crunch (2× OS), glue, room, width
  │    or
  ├─ Split: cascaded LR4 crossover → per-part shapers / crunch → sum
  ├─ Mix (dry/wet of the mode above)
  ├─ Drum bus (bypassed on Clean): punch, crunch (2× OS), squash, pump, room
  ├─ Output trim
  └─ Clip-safe ceiling (optional 1.5 ms lookahead)  → output
```

Default latency is 0 samples. Lookahead safety is off by default; when on, `setLatencySamples` matches the measured impulse delay. Oversampling is only on crunch, minimum-phase, and does not add integer latency. Clean and A/B dry are a bit-exact copy of the input (or a delayed copy when lookahead is on, so plugin delay compensation stays honest).

## Whole mode

Punch, Body, Snap, Crunch (Tape / Tube / Clip / Fuzz), Tone, Glue, Room, Width, Mix, Output, Ceiling. Starting points: Clean, Tight Punch, Fat Body, Lo-Fi Crunch, Wide Smash.

## Split mode

Cascaded Linkwitz–Riley 4th-order bands (low / mid / high) plus a transient split of the mid band into Snare and Perc. Per part: Level, Punch, Body, Decay, Crunch, Pan, Width, Listen, Mute, Sensitivity. Two split-frequency knobs (low and high).

Honest limits: this is a frequency + envelope split, not a stem separator. A kick’s click will bleed into Snare and Hats. Busy or already-crushed loops bleed more. Sensitivity helps; Listen shows you what each orb owns. Perc / toms often share the Snare or Perc band.

## Drum bus

Clean (default, true bypass), Tape Warm, Blown Speaker, Smashed Room, Bedroom Crunch. Punch, crunch, parallel squash, pump (low-band envelope) and room, all gain-compensated. The v1 hard clip is gone; the clip-safe ceiling at the end of the plugin is the safety.

## Loudness

Auto gain compensation on Punch, Crunch, Glue and Squash. Clip-safe ceiling default −0.1 dBFS: linear below 0.9× the ceiling, soft approach above, never exceeds the cap. Input and output meters, plus a hold light when the ceiling is working.

## UI

Beige, poster-orb language, one solid colour per part (kick orange-red, snare violet-blue, hats green, perc golden yellow). Resizable, high contrast. Whole | Split switch, A/B, reported latency when it is not zero.

## Tests

Engine and plugin tests cover: reported latency vs impulse, no output above the ceiling on stress signals (including Smashed Room maxed), Clean bit-exact null, Split listen/mute sanity on a synthetic kit, auto gain, every control, CPU budget, state round-trip, and pluginval at strictness 10 on VST3 and AU.

## Risks

Split mode will bleed. Zero-latency ceilings are not as polite as a 2 ms lookahead limiter; Lookahead is there for people who want it and will accept the reported delay. CRUMBS will not “find the drums” on a full mix — put it on the drum track you want shaped. None of this is verified in FL until it is opened and put on real drums.
