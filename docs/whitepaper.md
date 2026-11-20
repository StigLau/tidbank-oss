# TidBank white paper: architecture, audio routing, tape and BBD

Status 2026-11-15. Written after the tape engine core passed its ears (86/86) and the S8 listening set (approved). Decisions are recorded in `design.md`; this paper explains the why and the direction. Items marked VERIFY are unconfirmed facts.

Scope rule (D9, restated 2026-11-15): TidBank stays simple. Tape echo now, BBD maybe. Age, wow/flutter, hiss come from a separate tape-ageing plugin in the DAW chain. Reverb and other effects are separate plugins later.

## 1. Audio routing

One mono engine per channel (D8). Per sample:

```
in ──┬───────────────────────────────────────────────────────────► dry_level ──┐
     │                                                                          (+)──► out
     │         ┌──────────── feedback: HPF → LPF → × intensity ◄─────┐          │
     │         ▼                                                      │          │
     └──► (+) ──► record head: drive, tanh(gx)/g, ADAA ──► TAPE ring ─┼─► head 1 ─┤
                                                         (written at │─► head 2 ─┼─► wet sum y ─► echo_level ──┘
                                                          speed v)   │─► head 3 ─┘
                                                                     └── feedback = y / Σ head weights
```

- Tape = one circular buffer written once per sample. The speed `v` only changes *where on the tape* each head is (D1'): head `i` sits `d_i` tape-seconds after the record head, its delay `tau_i` solves `X(t) - X(t - tau) = d_i`. Changing tempo glides the pitch of the repeats like a real transport.
- Heads are read with a 32-tap windowed sinc (anti-image resampler; Hermite gave -18 dB images at any speed ratio != 1).
- Mode (1-12, provisional table) picks which heads sound and feed back; changes cross-fade 20 ms equal power.
- **Feedback is normalised** by the total head weight (2026-11-15): loop gain is at most `intensity` in every mode, so intensity 1 = edge of self-oscillation whether one or three heads play. Before this, three heads ran away at intensity 0.34, which is what made the S8 files with mode 4/7 build up. The wet output itself is not scaled. On a real RE-201 multi-head modes do run away sooner (VERIFY how much); if we want that back, it is one exponent (`y / wSum^k`, k=1 now) and a taste call.
- Stereo: two engines, L and R. Ping-pong later as an X of writer heads (Volante style, D8).

## 2. Latency: the echo pays for its own processing

What the user hears late is the *dry* path and the *timing* of the repeats. Both can be exact while the wet path still does work that needs look-ahead, because a head reads audio that was recorded at least `tau_min` ago. Everything the wet path needs is already in the buffer.

- **Dry path:** untouched, zero samples. The plugin reports `setLatencySamples(0)`; nothing on the dry path may ever add latency (no oversampling, no linear-phase filters there).
- **Wet path:** any processing with a fixed latency `L` is compensated by reading the head `L` samples earlier: `read(tau - L)`. Today: sinc read centred (16 samples of look-back, free), ADAA half-sample and read-before-push sample: `tau - 1.5`. Measured: repeats land within 0.0046 ms of the grid (`make ears`, S2 taps), so the processing adds no lateness.
- **Budget:** the shortest head is 40 ms = 1920 samples at 48 kHz. We use 17. That leaves room for heavy wet-path work: 2x/4x oversampling with linear-phase FIRs, FFT/block processing inside the loop (block size must stay below the shortest loop delay, since the record head needs the loop output within `tau` samples), BBD reconstruction filters, convolution.
- **The one place the budget runs out:** very short delays. A BBD chorus/flanger range (1-20 ms) or a looper "print" path (section 7) have little or no spare time. Rule: every filter that is not compensated by the read offset must be minimum-phase IIR (zero latency), like the HPF/LPF now.
- **Group delay of the loop filters** is not compensated: 0.037 ms per repeat (measured, S3 `s3_loop_gd`). After 10 repeats 0.37 ms: inaudible; could be folded into the read offset at the corner frequency if a tight 1/32 at high tempo ever needs it.

Hidden nuggets in the same vein (time, drift, position):
- **Tape position `X` is known exactly.** Anything tied to a *place on the tape* (a crease, a dropout, worn oxide) can be indexed by `X mod loop_length` and will then hit head 1, then head 2, then head 3 as that piece of tape passes them, at the right time even while the speed glides. That is how a real loop behaves and no stateless effect can fake it.
- **Speed and pitch are one variable.** A tempo change, a nudge, a "tape stop" and wow are the same `v(t)`; the engine already turns any `v(t)` into correct pitch on every head. A tape-ageing plugin's wow, if ever brought inside, would need no new maths.
- **Nudge is a detune, not a shift**: an echo has no absolute phase. Drift against the beat is the musical effect.

## 3. Software architecture

```
TidBank/
  dsp/      header-only C++17, JUCE-free, no allocation after prepare(), double state
            TapeTransport (speed with inertia, X/V rings, Newton tau solve)
            DelayLine     (ring, Hermite + sinc reads)
            FeedbackNetwork (HPF, LPF, intensity, ADAA saturator, smoothing, denormal flush)
            TapeEngine    (heads, modes, spacing glide, cross-fades, feedback normalisation)
            TapeParams    (plain struct; the wrapper fills it per block)
  tools/ears/  render.cpp (offline, deterministic), analyze.py, suite.py (86 cases), s8.sh
  tests/       ctest unit tests
  plugin/      JUCE wrapper (APVTS parameters in bank order, host tempo, pluginval; see design.md)
```

- Audio thread owns the engines. The wrapper copies APVTS values into a `TapeParams` per block; time-type parameters go through the transport inertia, the rest through 20 ms smoothers inside the engine. No locks, no allocation.
- Host tempo: `AudioPlayHead` bpm per block into `TapeParams.bpm`; the inertia makes host tempo jumps glide.
- MIDI CC on every knob (D5, superseded by d20: the host maps controllers; parameters are ordered in banks of 8).
- Everything that sounds is testable offline: ears render the same `dsp/` code the plugin runs.

## 4. Filters (proposed and in place)

| Where | Filter | Why | Status |
|---|---|---|---|
| Heads | 32-tap Kaiser windowed sinc, 0.45 fs, 256 phases | resampling without images when speed changes | in place. Cutoff is fixed: pitch-up above ~1.1x lets content near 0.45 fs/ratio fold (untested, small) |
| Loop | HPF 12 dB/oct TPT SVF, 20-500 Hz, 80 | stops bass build-up (mud) over repeats | in place, S8 approved |
| Loop | LPF 12 dB/oct, 1-12 kHz, 6 kHz | each pass darkens like real tape head loss; bounds HF in runaway | in place, S8 approved |
| Record head | ADAA `tanh(gx)/g`, drive -12..+18 dB | tape saturation, bounds runaway | in place; make-up gain is a taste question |
| Output | bass / treble shelves +-12 dB | RE-201 style tone after the echo | VERIFY with research item, not built |
| BBD only | anti-alias pre-filter, reconstruction post-filter, compander (2:1 / 1:2) | a BBD samples at `f_clk`; without these it aliases and hisses | D3, d18 |
| Looper only | input HPF/LPF before the tape | shape a source loop without changing it | expansion L1 |

All loop filters are minimum-phase IIR, so they cost no latency (section 2).

## 5. Tape and BBD: one frame, two media

The shared insight: a BBD is a tape whose speed is the clock. A sample enters stage 0 and leaves after `N` stages, i.e. after `N/2` clock periods: `tau = N / (2 f_clk)`. Count clock ticks as position and you get `X(t) - X(t - tau) = N/2`, the same equation the tape heads solve. So the transport (inertia, sync, nudge, Newton solve) is shared and only the *medium* differs:

| | Tape medium | BBD medium |
|---|---|---|
| Storage | continuous signal at host rate | `N` charge samples taken at `f_clk` |
| Delay change | head distance fixed, speed changes | stage count fixed, clock changes |
| Bandwidth | flat (head loss = loop LPF) | scales with delay: longer = darker (fixed AA/recon filters around the clock) |
| Noise/artifacts | none here (a separate plugin) | compander breathing, clock bleed, aliasing if filters are weak |
| Heads | 3 playback heads at `d_i` | usually one output; multi-tap chips exist (MN3011, VERIFY) |
| Inertia | the motor and tape | the time pot's RC / clock CV slew |

Recommendation:
- **Share the frame** (transport, sync/nudge, modes, feedback network, record-head drive, ears). Implement the medium as a swappable part: `TapeMedium` (today's ring + sinc heads) and `BbdMedium` (clock-ticked stage ring, Holters & Parker style, D3).
- **One medium per engine instance**, chosen by an engine switch (D4 option 1, d16). Mixing tape heads and BBD heads in one loop is possible with this structure but doubles the parameter surface for an effect nobody asked for. Keep separate in the product, shared in the code.
- **Later experiment, not v0.x:** "BBD in the tape's loop" (a short BBD stage in the feedback path) as the chorus/warble that some Volante settings imply. Only if A/B recordings say so.

BBD reference units: Moog Matriarch (two BBD lines, 35-780 ms per the manual, measured TIME 41.65-404 ms, bipolar SPACING between L and R, ping-pong cross-feed, Dark/Bright filter; manual facts in `research/bbd-matriarch.md`) and Strymon Volante, both available to the maintainer for measurement. The Matriarch's controls map onto the shared frame: SPACING = our `spacing`/`head_target` across two lines, ping-pong = the D8 X pattern, Dark/Bright = BBD reconstruction corner, Sync CV Bend = nudge. Chips, clock range, compander and filter placement are undocumented: measure them (section 8). Delay time, SPACING, filter and feedback were measured 2026-11-17 (`research/bbd-research.md` 8.1b); clock and compander are open.

## 6. Other effects and tape ageing

- **Reverb, modulation, anything else:** separate plugins in the DAW chain. TidBank does not grow a reverb. The only effect that would *need* to be inside is one in the feedback loop (each repeat through the reverb again), and that is not wanted now.
- **A tape-ageing plugin in front of TidBank** (the current choice): the source is aged once, then every repeat copies that aged sound. **After TidBank** (D9's wording): the whole output, dry and repeats, is aged once. Neither ages a repeat *more* each pass. Both are fine for v0.x; no new features needed.
- **Thoughts, not scheduled: the reused tape loop.** On an RE-201 the same short loop of tape passes the heads again and again (loop length VERIFY). Wear is then a property of the *tape*, not of the signal:
  - position-locked defects (dropout, crease, oxide wear) indexed by `X mod L`: each head hits the same spot in turn (section 2 nugget);
  - per-pass degradation (a little more HF loss, noise and saturation every time the signal is re-recorded) is already partly there: loop LPF and record-head drive act on every pass;
  - slow change over a session (an "age" knob drifting) could later steer the loop LPF and a position-locked noise map.
  If ever built, it is a hook in the feedback path that receives `X`, so it would be "the ageing plugin inside the loop". Until then, the ageing plugin goes in front.

## 7. Expansion L1: tape looper / rhythm composer (sketch 2026-11-15)

Idea: a base reel of 1, 4 or 16 bars ("boom ts cats ts") loops; the player overdubs (a synth on the 4th pass); a separate echo box treats the reel without changing it; "lifting the erase head" prints the echo box output back onto the reel.

Physical proposal (to test, not decided):
```
layers 1..n (kept raw, non-destructive) ──► reel mix (gain/mute per layer) ──► input HPF/LPF ──► echo engine ──► out
                                                       ▲                                              │
                                                       └──── "erase head lifted": print (level < 1) ◄─┘
```
- **Layers stay digital and separate**: every overdub and every print is its own buffer aligned to the loop start. What you hear is the render of the mix. Undo = mute a layer.
- **"Add or replace"** becomes two buttons on the same data: *add* keeps old layers, *replace* mutes them (still undoable); an explicit *flatten* collapses layers to one to save memory. We test which one feels like tape.
- **Loop length** = bars x 4 beats x 60/bpm x sr. At tempos that give fractional samples the loop re-aligns to the host's bar position each pass, so it never drifts against the DAW. Time signatures other than 4/4 later.
- **Printing closes a second feedback loop** (reel -> echo -> reel). It needs a print level below 1 and the same pull-back logic as intensity, or repeats pile up exactly like the S8 runaway file. Echo tails that cross the loop end wrap to the loop start, like on a tape loop.
- **Latency**: printing is time-critical; zero-latency engine (section 2) means the print lands on the grid. Live overdubs rely on the host's input latency compensation.
- **Memory**: 16 bars at 60 bpm = 64 s, about 12 MB per mono layer at 48 kHz float; 16 layers stereo about 400 MB. Cap layers or use 16-bit storage. Allocation happens on record-arm, never on the audio thread.
- Open when written: is the looper inside TidBank or a separate plugin that hosts the echo engine? Later decided (2026-11-17) as a separate reel-only plugin (see `plan-looper.md`).

## 8. Measuring real units (Volante, Matriarch)

The analyzers read any WAV, not only renders. That makes the hardware the ground truth:
1. Record from each unit (dry/wet 100 % wet if possible, 48 kHz): an impulse/click train, the `burst` stimulus, a 1997 Hz tone, a sweep, and a tempo/time knob turn during a held tone.
2. Run `taps`, `bursts`, `spectrum` per repeat, `thd`, `glide` on the recordings.
3. Fit our parameters (inertia, spacing ratios, loop filter corners, drive curve, BBD bandwidth vs delay) to the numbers, then confirm by ear.
This replaces most of the VERIFY marks with measurements. A recording checklist (levels, settings per take) can be prepared for whoever does the measuring.

## 9. Open questions (as of 2026-11-15)
- Multi-head runaway: keep the normalised feedback (now) or bring back the "runs away sooner with more heads" hardware behaviour?
- Looper (L1): inside TidBank or its own plugin?
- Repo move to its own repository (decided, d19).
