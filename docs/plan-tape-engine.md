# Plan: TapeEngine (v0.1)

Goal v0.1: steps S0-S4 and S7 are checked by numbers, S8 by listening. Decisions ACCEPTED: D1' = d13, D2' = d14, D8 = d11, D9 = d12 (see `design.md`). Each step ends with ears assertions an agent can run; "done" means those numbers, not opinion. Thresholds were chosen to catch bugs; loosen only with a reason written here. Review: a second model reviewed the plan on 2026-11-15 (16 points, folded in below; its simulations agree with the S2 numbers measured on the engine).

## Agreed scope
- Exact tape model (D1'), head targeting + cents nudge (D2'), two mono engines (D8), no wow/flutter/age/spring reverb in TidBank (D9: a separate tape-ageing plugin goes after it in the chain). Consequence of D9: modulation then hits the whole echo bus uniformly, not each repeat as it passes the heads. Revisit if that sounds wrong.
- RE-201 mode table: provisional head bitmask until research item 2 returns, flagged `// PROVISIONAL` in code.

## Model and conventions
- `v(t)` = relative tape speed, 1.0 nominal. `X(t) = integral v dt` in tape-seconds, double, one value per sample in a ring (no rebase needed: at 1e5 s the ulp is ~1e-11 s). Head `i` sits `d_i = ratio_i * D1` tape-seconds from the record head, `D1 = 0.5 s`, ratios from the spacing preset.
- Delay `tau_i(t)` (samples) solves `X(t) - X(t - tau) = d_i`. One Newton step per sample from the previous `tau`: `tau -= (X(t) - X(t-tau) - d_i) / v(t - tau)` (units: X in tape-samples, i.e. `X` advances by `v` per sample). `X(t-tau)`, `v(t-tau)` by linear interpolation of the rings. Pitch of a repeat of a steady tone is `v(t)/v(t-tau)`.
- History at `prepare()`/`reset()`: the X ring is prefilled as if the tape had always run at the initial `v0` (`X(t) = v0 * t` for t < 0), `tau_i = d_i / v0`, audio ring zeroed. No special case for "buffer not full yet".
- Inertia: two cascaded identical one-poles on `v`, `tau_pole = inertia_ms / 6.638` (99 % of a step at `inertia_ms`, critically damped, no overshoot).
- Sync: `v_target = d_target / tau_target(bpm, division)` for the head `head_target`; nudge multiplies `v_target` by `2^(cents/1200)` (a detune of tempo, not an alignment). Free mode: `tau_target = time_ms`.
- Clamps: `v_target` in `[D1/2.0 s, D1/0.04 s] = [0.25, 12.5]`, applied AFTER nudge, so head 1 stays in 40 ms-2 s whatever head is targeted. When the clamp binds, sync is broken (e.g. 1/32 at 180 BPM on head 3 needs v = 36): the engine exposes `isClamped()` for the UI. `tau` clamped to [2, buffer - 4]. Buffer: 8 s at any rate (head 3 at v=0.25 is 6 s, vintage 5.5 s, plus interpolator/Newton margin).
- Causality guard: `tau` grows by at most 1 sample per sample (pitch >= 0). Binds only when one Newton step overshoots (inertia far below the 30 ms minimum with big jumps; measured: 1 ms, 0.25 -> 12.5 gives ratio -25 without it, 0 with it).
- Per-sample order: advance `v` and `X` -> solve each `tau` -> read heads -> (S3) feedback filters -> write `sat(in + fb)`. The first `setParams()` after `prepare()`/`reset()` snaps `v` and `tau` to the target (no start-up glide).
- Interpolation: `DelayLine::read(double)` (float positions lose 1/32 sample above 262144 samples). Hermite images at ANY ratio != 1 (review: alias -17.8 dB for a 15 kHz probe at ratio 1.25 and also at 0.8), so heads read through a 32-tap windowed sinc (`readSinc`, Kaiser, cutoff 0.45 sr) for all ratios. Content pushed above Nyquist by ratio > 1 still folds; accepted (short glides only).
- Record head writes `sat(in + fb)`. Feedback = sum of the mode's active heads -> HPF -> LPF -> `intensity`. No wow/flutter/age.
- Engine interface (mono, no allocation after `prepare`): `prepare(sr, maxDelaySec)`, `reset()`, `setParams(const TapeParams&)` once per block (also per sample in the renderer's schedule), `float process(float in)` returns wet only. Dry/wet mix lives outside the engine.
- No `-ffast-math`. Renders are deterministic: same args, same bytes.

## Ears additions (shared by the steps)
- Render CLI (S0): `--engine naive|tape`, `--param key=value` (repeatable via comma list `--params k=v,k=v`), `--sched "3.0:bpm=100;6.0:nudge=50"` (parameter changes at times in seconds, applied at that sample), `--probe v|tau1|tau2|tau3` (write the internal state instead of audio, for transport checks), `--stim burst` (1 kHz tone bursts, 20 ms, at 0.1 s, for per-tap level).
- Suite runner (S0): `tools/ears/suite.py`, a table of cases (render args, analyzer, assertion), coloured PASS/FAIL per case, non-zero exit on failure, summary line. `run_ears.sh` becomes a thin wrapper; `make ears` runs everything, `make ears CASE=s2` a subset by prefix. Each step adds its cases here.
- `alias` analyzer (S2): two-tone stimulus (`--stim twotone`, 1 kHz pilot + probe `fp`). Pilot gives `ratio(t)`; expected probe at `ratio*fp` (absent if above Nyquist). Reports `alias_db`: power in 200 Hz-Nyquist outside +-3 % of the expected probe and pilot harmonics, relative to input probe power, in frames where `|ratio - 1| > 0.001` (or all frames with `--all-frames`). Self-test with a known alias, including one folding to 500 Hz.
- `step` analyzer (done): overshoot and 99 % settle of a probe signal. `glide` takes `--f0` as reference and a `--start-s/--end-s` window (the first 0.5 s of a wet-only render is silence).
- `bursts` analyzer (S3): RMS of tone bursts at `onset + k*period`, per-repeat `step_db` (`taps` finds several peaks per burst).
- `spectrum --win-ms` (S3): 20 ms cannot resolve 100 Hz (review: -3.14 vs -1.48 dB/repeat); use 100 ms.
- `--probe tauerr1..3` = `tau - d/v` (small numbers keep float32 precision; plain `tau` has a 0.002-sample ulp at 24000) and `--probe clamped`.
- `thd` gets `inharmonic_db` (power outside harmonics +-4 bins). THD alone is toothless for delay reads (a linear read makes sidebands, not harmonics) and for aliasing at f0 = sr/n (aliases fold onto harmonics): use a non-divisor f0 such as 1997 Hz.
- Every new analyzer goes into `selftest.py` in the same change.

## Steps
**S0 Interfaces and plumbing** (Sonnet, DONE). `TapeParams` (plain floats), `TapeEngine` skeleton returning silence, render flags above, `suite.py`. Accept: `make test` and `make ears` green; hash of `--engine naive` output equals the hash taken before the change (record it in the commit message).

**S1 DelayBuffer with a moving read point** (Sonnet, DONE). Reuse `DelayLine` (Hermite), `read(double)`, semantics documented in code. Unit tests: ramp read exact at fractional delays; constant integer delay hits exactly; a 1 kHz sine read through a slow delay sweep (0.5 -> 0.51 s over 4 s): THD -157 dB (Hermite). Add `readSinc` with the same tests plus inharmonic < -100 dB (Hermite floor about -92 dB per the review). Accept: tests green.

**S2 TapeTransport + exact single head** (Opus, DONE: 30 cases, `make ears CASE=s2`). `v` with inertia, X/v rings, Newton `tau`, sync mapping, nudge, free `time_ms`, clamps. No feedback. Step the tempo AFTER 3 s (buffer valid). Ears, with `--params head=1|2|3` soloing a head (ratios 1:2:3):
 - steady: `taps` at `ratio_i * tau1` +-0.01 ms at 90/120/128 BPM, fractional delays (97 BPM, 44.1 kHz) and nudged (+100 c, -400 c on head 3). `--probe tauerr1..3` steady within 1e-3 samples. (+-0.01 ms holds after S3 only if the ADAA half-sample is compensated, see S3.)
 - step 500 -> 450 ms on head 1 (v 1 -> 1.111), inertia 300 ms: `area_s` == `tau_old - tau_new` +-2 % on every head (necessary, not sufficient).
 - distinguishing, per head: `max_ratio` == 1.111 +-1 % on EVERY head (rejected model: 1.43 / 1.86 / 2.28); reverse step 450 -> 500: `min_ratio` == 0.900 +-1 % (rejected model reverses); glide duration == `tau_new_i + inertia` +-(50 ms + 2 %) (exact model: 0.75 / 1.20 / 1.65 s; the rejected model ends with the speed change, ~inertia, on every head). Peak and duration valid only while `inertia < tau_i`; at inertia 2 s assert area only.
 - `--probe v` (`step`): no overshoot beyond target (> 1e-6 relative), 99 % settle within +-2 % of `inertia_ms` (30, 300, 2000 ms).
 - `--probe clamped`: 1/32 at 180 BPM with `head_target` 3 reports 1; 1/8 at 120 reports 0.
 - inertia sweep 30 ms / 300 ms / 2 s: area unchanged +-2 %. At 30 ms also `max_ratio` within 1 % (guards Newton lag; fall back to 2 iterations if not).
 - `inharmonic_db` of a 1997 Hz tone through a static fractional head (97 BPM) < -85 dB.
 - aliasing, through the sinc read: pilot + 5 kHz probe, 500 -> 400 ms step (ratio 1.25) and back (0.8): `alias_db` < -60 dB; 15 kHz probe < -40 dB. Hermite gives -17.8 dB at 15 kHz (review), so this is what forces the sinc read. Larger steps are reported, not asserted.

**S3 FeedbackNetwork** (Sonnet + Opus, DONE: `make ears CASE=s3`; gd 0.037 ms/repeat, burst steps -6.04 dB, HPF per repeat within 0.22 dB of the band mean of |H|^2k (the expectation is NOT k x the one-pass value: steep slopes weight the band's upper edge), THD -58 -> -11 dB over drive -12..+18, inharmonic -56 dB at +18, 60 s runaway bounded at +9.7 dBFS). HPF 12 dB/oct (`fb_hpf`, default 80 Hz), LPF 12 dB/oct (`fb_lpf`, chosen here, 1-12 kHz, default 6 kHz: repeats darken), saturator at the record head on `in + fb`: `tanh(g x)/g` (unity small-signal gain, so `intensity` IS the loop gain; `g` from `drive`), first-order ADAA with the `|dx| < 1e-5` fallback. ADAA delays the written signal by 0.5 sample: read heads at `tau - 0.5` to keep S2's +-0.01 ms. `intensity`, `drive` and filter cutoffs smoothed (one-pole, 20 ms). Feedback sums the same heads with the same crossfade gains as the output. Denormal guard (flush to zero or 1e-20 offset in the loop). Ears:
 - loop timing: impulse, head 1, 6 repeats: tap k at `k * tau1 + k * gd`; assert `gd` < 0.1 ms at the default filters (review: ~0.038 ms from the filters). At `fb_lpf` 3 kHz gd ~0.075 ms; below ~1.5 kHz peak-picking is unstable: the rule is for defaults only, other settings report gd.
 - level: `--stim burst` at fixed level 0.25, drive 0 dB, intensity 0.5: `bursts` `step_db` == -6.02 dB +-0.3 (|H_hpf H_lpf|(1 kHz) is -0.003 dB).
 - `spectrum --win-ms 100`: after k repeats, shape at 100 Hz vs tap 1 follows `|H_hpf(100)|^k` (band-averaged) within 0.3 dB per repeat; `fb_hpf` 20 Hz gives < 0.2 dB/repeat.
 - `thd` vs `drive` (-12, 0, +6, +12, +18 dB) at 1997 Hz, amplitude 0.5: THD monotonic, H3 > H2 (symmetric saturator); `inharmonic_db` < -45 dB at +18 dB (review: naive tanh -44.9, ADAA1 -50.3; -60 would need 2x oversampling, not planned).
 - `intensity` 1.2 for 60 s, noise in for 1 s then silence: `health` ok, peak over 30-60 s <= peak over 10-30 s + 1 dB (bounded, not growing).

**S4 Three heads, head_target, spacing, modes** (Opus, DONE: `make ears CASE=s4`; 18 tap cases exact, clicks -82..-108 dBFS, all 12 modes healthy at intensity 1.0. A spacing change jumped the heads (click -26 dBFS): head distances now glide through the inertia filter like speed). Spacing presets even 1:2:3 (default) and vintage 1:1.9:2.75 (provisional, research item 1). `head_target` chooses the head the division/time sets (1/16 base with heads 2:3 = 1/8 + dotted 1/8). Mode table: 12 entries of head bitmask (`// PROVISIONAL`), output = sum of active heads, 20 ms equal-power crossfade per head on mode change. Ears:
 - each head tap at `ratio_i * tau_target / ratio_target` +-0.05 ms, for `head_target` 1, 2, 3 and both presets.
 - nudge +100 cents scales all head times by `2^(-100/1200)`, error < 0.01 %.
 - mode change mid-tone (-20 dBFS, 500 Hz): high-pass the output at 2.5 kHz (5x the tone), peak within +-50 ms of the switch < -60 dBFS. (A difference against a no-switch render is wrong: the outputs legitimately differ by a whole head.)
 - all 12 modes x intensity 1.0, 30 s: `health` ok.

**S5 DROPPED** (wow/flutter, a separate ageing plugin downstream). **S6 DROPPED** (age, HF loss, hiss, dropouts). Numbering kept. If they come back, the review's notes apply: flutter rate scales with `v`, wavelength-dependent loss and head-bump corners scale with `v`, and the `v` seen by them is clamped to a plausible range.

**S7 Stability** (Opus, DONE: `make ears CASE=s7`; 300 s at intensity 1.2, drive +18, inertia 30 ms with tempo/nudge/mode changes: finite, no subnormals, bounded; 0.71 % of real time per mono engine on an Apple-silicon Mac). 5-minute renders at extreme settings: intensity 1.2, drive +18, inertia 30 ms, `--sched` tempo jumps every 2 s between 60 and 180 BPM, nudge sweeps +-400 c, mode changes every 3 s. `health` ok (finite, no subnormals), peak bounded as in S3, render time < 5 % of real time per mono engine on the development Mac (record the number: the CPU budget for the plugin).

**S8 Human gate** (a human listener). `make s8` renders a synthetic drum loop (or `S8_IN=` a real one) to `build/s8/` (gitignored); checklist `docs/s8-listening.md`. The listener decides. Only then tag `v0.1-dsp` and plan the JUCE wrapper.

## Delegation
- Mechanical (CLI plumbing, analyzers with a precise spec, tests, stability renders): Sonnet subagent, brief = this step + the skills `tidbank-ears` / `tidbank-tape-engine`.
- Model core (TapeTransport, any change to the Newton solver): Opus, because a subtle error here passes casual tests (the quasi-static flaw did).
- Stuck after two attempts on a numeric failure, or a disagreement between model and measurement: a stronger review model, given the failing numbers, not a hypothesis.

## Out of scope here
BBD engine, spring reverb, JUCE wrapper, MIDI learn, editor, stereo ping-pong (X pattern, later), 14-bit MIDI, wow/flutter/age (a separate ageing plugin downstream), output bass/treble EQ (wrapper stage; placement on the RE-201 is research item 3).

## Risks
- Speed range: 0.25-12.5 here vs about 2.5:1 on a real RE-201 (review, UNSURE). Harmless while nothing is speed-coupled; settle before adding anything that is.
- Newton lag at 30 ms inertia with large steps: S2 asserts `max_ratio` at 30 ms; fall back to 2 iterations.
- Interpolator imaging during glides: sinc read for all heads (S2). Cost 32 MACs per head per sample; S7 measures CPU.
- Inertia 300 ms, spacing ratios, mode table, `fb_lpf` default: unmeasured estimates. S8 and the open research items validate them.
