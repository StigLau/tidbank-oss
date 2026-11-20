# TidBank design

Decisions are numbered dNN (D1'=d13, D2'=d14, D8=d11, D9=d12 decided by the maintainer 2026-11-15; D5=d15, pluginval=d17, D4=d16, D3=d18 decided later that day). This file is the written record of them.

Status: D1', D2', D8, D9 ACCEPTED; D3-D7 proposed. Source: a design review by one model (2026-11-14) and a second review by another (2026-11-15), from their own knowledge, no measurements yet. Items marked VERIFY are open research.

## Decisions (D1-D7)

| # | Decision | Confidence | Status |
|---|---|---|---|
| D1 | ORIGINAL (superseded by D1'): tape speed state with inertia, quasi-static `tau = d/v`. Rejected by the second review 2026-11-15 and reproduced numerically: heads 2-3 pitch-glide 2x too hard for a fraction of the time, slow-downs reverse playback, flutter depth scales with head distance. | - | superseded |
| D1' | Tape time dynamics: speed `v` with inertia (two cascaded one-poles, 99 % settle default 300 ms, 30 ms-2 s); tape position `X = integral v`; each head's `tau` solves `X(t) - X(t-tau) = d_i` with one Newton step per sample (exact, pitch of repeat = `v(t)/v(t-tau)`). Heads read through a 32-tap windowed sinc (Hermite imaged at any speed ratio != 1; approved 2026-11-15). Nothing speed-coupled in TidBank v0.1 (wow/flutter/age belong to a separate tape-ageing plugin later in the chain). | high (derivation in the review + a numeric check) | ACCEPTED 2026-11-15 (d13) |
| D2 | ORIGINAL (superseded by D2'): heads 1:2:3, division on head 1, nudge +-25 % linear-ish x^2 with deadband. | - | superseded |
| D2' | Heads: spacing preset (even 1:2:3 default, provisional vintage 1:1.9:2.75, VERIFY); the division/time targets a chosen head (`head_target`) so 2:3 patterns work; nudge = exponential in cents (about +-400 c, momentary-style use, no deadband); nudge is a detune of tempo, not an alignment (an echo has no absolute phase). Max head-1 time 2 s. | med | ACCEPTED 2026-11-15 (d14) |
| D3 | BBD: clock-tick-sampled ring of N stages + fixed AA/reconstruction filters + NE570-style compander + clock noise (follow Holters & Parker, DAFx 2018, VERIFY). Defer charge-transfer loss to v0.3, skip circuit-level. `f_clk = N/(2 tau)`. Clamp to chip range, optional "Extended" underclock. Never auto-switch stage count. Chip choice is user's. | high | DECIDED 2026-11-15 (d18): build all three model levels (clock-domain, simple, circuit-level) as separate paths behind a UI selector, compare by ear, then drop the losers |
| D4 | One plugin, engine switch (50 ms crossfade, both run during fade). Engines share code, not state. Nonlinearity at the record head on `in + fb`; HPF in feedback before the sum. ADAA first-order at host rate; 2x oversampling only if measured aliasing. | med | ACCEPTED 2026-11-15 (d16): one plugin, one codebase; how much code is shared is left to the implementers, as long as tape and BBD do not entangle |
| D5 | MIDI: atomic 16x128 CC->param table, learn via atomic param id, CC applied to DSP smoother on audio thread, SPSC FIFO to a 60 Hz message-thread timer for host/UI. Persist in APVTS ValueTree. 7-bit only in v0.1. `NEEDS_MIDI_INPUT TRUE`, AU `aumf`. Host automation stays primary path. | med, host matrix unverified | SUPERSEDED 2026-11-15 by d20 (after testing with Ableton Move in Ableton Live): no MIDI in the plugin; the host maps controllers; parameter order = banks of 8 (bank 1 Intensity, Division, Mode, Nudge, Inertia, Loop LPF, Loop HPF, Echo; bank 2 Time, Drive, Spacing, Target head, Sync, Tempo, Dry, Output; bank 3 Panic. 2026-11-17: Tempo removed (dead in Live), so bank 2 = Time, Drive, Spacing, Target head, Sync, Dry, Output, Panic; bank 3 = Engine, BBD profile, BBD bright, BBD spacing, Ping-pong) |
| D6 | Start order (below): ears + dsp core first, plugin wrapper after. | - | proposed |
| D8 | Stereo: two independent mono engines (L/R). Ping-pong LATER as writer heads on the other channel's tape in an X pattern (Volante style). | - | ACCEPTED 2026-11-15 (d11) |
| D9 | Scope: spring reverb later still; wow/flutter, age/loss/hiss/dropouts (old S5, S6) are NOT in TidBank; a separate tape-ageing plugin goes after it in the DAW chain. | - | ACCEPTED 2026-11-15 (d12) |
| D7 | Build: TidBank is a standalone CMake project in its own repo (d19). dsp/ is JUCE-free so tests/ears build in seconds. JUCE wrapper enabled by `TIDBANK_BUILD_PLUGIN`. | - | proposed |

## Layout
```
CLAUDE.md  README.md  CMakeLists.txt  Makefile   `make test | ears | s8`
.claude/skills/   tidbank-architecture, -tape-engine, -ears, -rt
dsp/              header-only, JUCE-free, RT-safe: TapeTransport, DelayLine, FeedbackNetwork, TapeEngine, TapeParams, NaiveEcho (reference)
tests/            ctest unit tests
tools/ears/       render.cpp, analyze.py, selftest.py, suite.py (cases), run_ears.sh, s8.sh, drumloop.py
plugin/           JUCE wrapper: PluginProcessor (APVTS in bank order, 2 mono engines, host tempo, panic), PluginEditor (plain controls)
tools/ui/         snapshot.cpp: offscreen editor PNG (`make ui`)
docs/             whitepaper.md (why), design.md (this: decisions, parameters), plan-tape-engine.md (steps + numbers),
                  s8-listening.md, rt-audio-checklist.md, research/ (bbd-matriarch.md, bbd-research.md)
```

## Order of work
1. DONE: ears harness, analyzers with self-test.
2. DONE: TapeTransport + TapeEngine + FeedbackNetwork (S0-S4, S7 ears; S8 listening approved).
3. DONE 2026-11-15: JUCE wrapper: APVTS, host bpm via AudioPlayHead, MIDI CC (d15), pluginval strictness 5 (d17), one/two plugins (d16).
4. BBD: three model paths behind a selector (d18), Matriarch-steered (d16), as a second medium in the same frame (whitepaper section 5); measure Volante/Matriarch first (section 8).
5. Editor (Blender-rendered filmstrips are one option).
6. L1 looper (`plan-looper.md`).

## v0.1 tape parameters (revised for D1', D2', D9)
`sync` bool on | `division` 1/32-1/2 straight/dotted/triplet, 1/8 | `head_target` 1-3, 1 | `spacing` even/vintage | `nudge` cents about +-400, 0 | `time_ms` 40-2000 log, 150 (target head, sync off) | `inertia` 30-2000 ms log, 300 | `mode` 1-12, 4 | `intensity` 0-1.4 (1.2 until the intensity range was re-decided), 0.45 (loop gain <= intensity in every mode: feedback is the head sum / head weight) | `bass`/`treble` +-12 dB (VERIFY) | `fb_hpf` 20-500 Hz log, 80 | `fb_lpf` 1-12 kHz log, 6000 (loop darkening, approved after listening 2026-11-15) | `drive` -12..+18 dB | `echo_level` 0.5 | `dry_level` 1 | `output` -24..+6 dB.
Removed vs first draft: `wow`, `flutter`, `age` (a separate plugin), `reverb_level` (spring reverb later). Smoothing: 20-100 ms per parameter; time-type params go through the inertia model instead.

## BBD direction (notes on d16/d18, 2026-11-15)
- One plugin, one codebase, tape and BBD as separate media in one frame (whitepaper section 5). Share what fits, never entangle them.
- **Matriarch is the steering concept for BBD**: two BBD lines, their filtering and timing (Spacing between them, ping-pong, Dark/Bright). See `research/bbd-matriarch.md`.
- Where BBD sits is open: "anywhere on the timeline" (instead of tape, beside it, in the loop). Decide by user testing, not up front.
- Model level (d18): three paths side by side with a selector (clock-domain, simple, circuit-level), A/B by ear and by ears, then remove one or two. The UI layout (engines aligned or together) is also for user testing.
- "We are learning": keep BBD work experimental and cheap to delete until the comparison is done.

## BBD profiles (d21, 2026-11-15)
- **Fixed wiring, profile data.** Chain in code: compressor, anti-alias filter, BBD, reconstruction filter, expander, feedback. A profile holds the numbers: stages, clock range, chip family (headroom, noise, THD), filter values or poles, compander on/off and C_RECT, nonlinearity coefficients, model path (d18), plus the targets it must measure to (max delay, clock at max, -3 dB point). Facts per pedal: `research/bbd-research.md`.
- **Order.** First profile = Matriarch, as a C++ struct with defaults, measured against the maintainer's unit. Move to a JSON format when a second profile exists, not before.
- **Who edits.** Agents write profiles, guided by project documentation, and run ears per profile as the safety net; an experimenting user can drive that agent (HITL). Users otherwise get presets plus performance knobs. No profile or wiring editing in the UI and none while running. A read-only wiring diagram in the UI is welcome.
- **Fixed host parameters.** A profile changes what knobs mean, never which knobs exist (Move banks, d20). Switching profiles is not automatable.
- **No modular patching** (block graph): costs RT safety, CPU and test combinations for variation real pedals don't have.
- **Generic preset names**, no trademarks; only the Matriarch can be checked against a real unit, the rest are "plausible".
- **Later, not a requirement now:** switch profiles live by running the old one until the new one is ready, then crossfading.

## Later (agreed, not scheduled)
- Ping-pong / stereo: writer heads of each channel record onto the other channel's tape (X pattern), as the Volante does.
- Spring reverb (as a separate plugin, agreed 2026-11-15). BBD engine (D3; reference units Matriarch + Volante, `research/bbd-matriarch.md`). 14-bit MIDI/NRPN.
- L1 tape looper / rhythm composer: layered reel of 1/4/16 bars, echo box on top, "lift the erase head" prints back. Design sketch: `whitepaper.md` section 7.
- Tape-position-locked wear (a tape-ageing plugin inside the loop): thoughts only, `whitepaper.md` section 6. For now: the ageing plugin goes in front of TidBank in the chain.

## pluginval (d17, 2026-11-15)
Reintroduce pluginval, with the aim of making it future-proof on macOS without Rosetta (native arm64 build, fix or patch what relies on old tech). Agents should run the plugin and look at the UI themselves rather than leaving all testing to a human in Ableton Live.

## Known gaps
- Nothing here was measured against a real RE-201/Volante. Inertia 300 ms and the spacing ratios are estimates; tune by ear and recordings.
- Review trail: a first model produced D1-D7 on 2026-11-14; a second model reviewed D1/D2 on 2026-11-15 and found the quasi-static flaw (D1', D2'). Speed range (0.25-10 for D1=0.5 s vs ~2.5:1 on a real unit, UNSURE) is open.
- The 12 RE-201 mode table is not written down (research item).
