---
name: tidbank-tape-engine
description: How to build and verify TidBank's TapeEngine step by step (transport/inertia, heads, feedback loop) with the ears. Use when writing or changing anything in dsp/ that is part of the tape engine, or before claiming a tape-engine step is done.
---

# Building TapeEngine

Plan with per-step acceptance numbers: `docs/plan-tape-engine.md`. Decisions: `docs/design.md` (D1', D2', D8, D9 ACCEPTED). Measuring: skill `tidbank-ears`. Audio-thread rules: `tidbank-rt`. Layout and order: `tidbank-architecture`.

## Workflow per step
1. Read the step in the plan. Write the ears assertion FIRST (in `run_ears.sh` / a test) and watch it fail, then write the DSP.
2. Implement in `dsp/` only (header-only, JUCE-free, no allocation after `prepare`).
3. `make test && make ears`. A step is done when its numbers pass, not before. Report the numbers.
4. Update this skill with any trap you hit, in the same commit. If a threshold had to loosen, say why in the plan.

## Model in one paragraph
One speed state `v` (1 = nominal) follows its target through two cascaded one-poles (critically damped, no overshoot), `tau_pole = inertia_ms / 6.638` so `inertia_ms` = time to 99 % of a step. Tape position `X = integral v dt` (double, one value per sample). Head `i` is `d_i` tape-seconds from the record head and its delay `tau_i` solves `X(t) - X(t - tau) = d_i`: ONE Newton step per sample, `tau -= (X(t) - X(t-tau) - d_i) / v(t - tau)`, with linear interpolation into the `X` and `v` rings. Sync sets `v_target = d_target / tau_target` for the head chosen by `head_target`; nudge multiplies by `2^(cents/1200)`. The record head writes `sat(in + fb)`; the feedback path is HPF -> tone LPF -> gain `intensity` before the sum. No wow/flutter/age here: a separate tape-ageing plugin does that downstream.

## Facts to keep straight
- Pitch of a repeat of a steady tone is `v(t) / v(t - tau)`. It is bounded by the speed ratio and lasts `tau_i` after the glide. It is NOT `1 + d v'/v^2` (the quasi-static `tau = d/v` form): that overshoots 2x on head 3, and reverses playback on slow-downs. Second-model review 2026-11-15; reproduced numerically (head 3, 500->450 ms: 2.28 vs 1.111; slow-down ratio -0.16).
- `area_s == tau_old - tau_new` holds for BOTH models, so it cannot tell them apart. Always also assert `max_ratio` equals the speed ratio on every head and `min_ratio > 0.9` on a slow-down.
- Inertia is on speed so time changes glide and click-free; never slew the delay in samples directly.
- An echo has no absolute phase: nudge detunes from tempo, it does not "align" anything.
- Stereo is two independent mono engines. Ping-pong later via an X pattern of writer heads (Volante).

## Traps (add more)
- Read-before-push feedback loops: delay = `read(d)` + 1 sample. Subtract 1 or the sync is a sample late per repeat (the ears caught this, 0.0208 ms/tap at 48 kHz).
- Filters in the loop add group delay, so repeat k lands later than `k*tau1`. Measure the offset with `taps`, compensate it or document it. Never loosen the 0.05 ms tolerance silently.
- Spectrum analysis windows must be centred on the tap and shorter than the tap spacing (an edge-placed Hann window biased the self-test by 1 dB).
- Denormals in the feedback loop at long decays: flush or add a tiny DC offset; `health` reports `subnormals`.
- The mode table is PROVISIONAL until research item 2 (RE-201 12 modes) is answered. Do not present it as faithful to the real unit.
- Wow/flutter/age/reverb are out of scope (D9). Do not rebuild it here; a separate tape-ageing plugin is chained after TidBank.
- Test glides with the tempo step AFTER the buffer has filled (>= 3 s for head 3). A mask that includes the not-yet-valid region silently truncates the measurement (happened in a cross-check script).

- Delay positions in float lose fraction: at 262144+ samples the step is 1/32 sample (-58 dB sweep error). `DelayLine::read` takes double; keep every tau in double.
- Hermite images at ANY speed ratio != 1, not only pitch-up (alias -17.8 dB for a 15 kHz probe at 1.25 and at 0.8). Heads read through the windowed sinc.
- Glide duration of head i is `tau_new_i + inertia`, NOT 1:2:3 across heads. This is also what tells the exact model from the quasi-static one (whose glide ends with the speed change on every head).
- `glide` on a wet-only render needs `--f0` and `--start-s/--end-s`: the default reference is the first 0.5 s, which is silence before the first echo.
- One Newton step per sample is exact within the 30 ms-2 s inertia range even for 0.25 <-> 12.5 jumps, but overshoots into reversal far below 30 ms. The transport caps tau growth at 1 sample/sample (pitch >= 0).
- Probing `tau` through a float32 WAV: ulp 0.002 samples at 24000. Probe `tauerr` (`tau - d/v`) for precision checks.
- THD cannot see delay-read errors (sidebands, not harmonics) nor aliasing at f0 = sr/n (folds onto harmonics). Use `inharmonic_db` at a non-divisor f0 (1997 Hz).

- Single-head (S2) cases must set `intensity=0`: the default 0.45 feeds back and wrecks glide/area measurements.
- Anything that changes a head distance (spacing preset) must glide, like speed: an instant jump clicked at -26 dBFS. Tempo/target/nudge are click-free because they go through the inertia.
- Expected per-repeat filter shapes: use the band mean of |H|^(2k), not k times the one-pass band value (wrong by 0.9 dB after 4 passes at a 300 Hz HPF).
- `tanh(g x)/g` drive lowers the ceiling (repeats get quieter with drive) and gives -34 dB THD at drive 0 with a 0.5 input: taste call for S8, not a bug.

- Feedback must be normalised by the head weight (`y / max(sum w_i, 1)`): the raw sum gave loop gain = intensity x heads, so mode 7 ran away at 0.34 and S8 files built up (noticed by the listener). `s3_loopgain_mode4/7` guard it.
- Latency: the plugin reports 0; wet-path look-ahead is paid from the echo delay by reading `tau - L`. Never put latency on the dry path. See `docs/whitepaper.md` section 2.

## Not yet written
The analyzers have no aliasing measure (needed if Hermite aliases on pitch-up glides) and no hiss/noise-floor command. Add them to `analyze.py` AND `selftest.py` together when a step needs them.
