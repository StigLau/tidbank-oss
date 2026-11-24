---
name: tidbank-ears
description: How an agent "hears" TidBank without listening - offline render of the JUCE-free dsp/ core and numeric measurement (echo timing, level, later spectrum/wow-flutter). Use when verifying any TidBank DSP change, timing/sync behaviour, or before claiming a sound change works.
---

# TidBank ears

Agents cannot listen. They measure. Evidence is numbers from a deterministic offline render, never a plot read by an LLM.

**Why numbers, not spectrogram images:** an earlier benchmark (not part of this repo) found vision-language models reading spectrograms/waveforms at chance level for exact times and frequencies. DSP gives reproducible timing, LLMs give semantics. Never ask a model to read a millisecond or Hz value off a picture. A plot is for the human; the agent asserts on JSON.
Perception bars used here: beat-layered timing <~10-15 ms (aim <5), echoes >50 ms are heard as separate events, gaps >5-10 ms click. (Those bars are rules of thumb from the same earlier work.) Sync tests here are much tighter than that (0.1 ms) because the DSP is deterministic, so any miss is a bug, not taste.

## Loop
1. `tidbank_render` (C++, links `dsp/` only, no JUCE) renders a stimulus (or `--in x.wav`) through an engine and writes float32 WAV. Deterministic, same input -> same bytes.
2. `tools/ears/analyze.py` measures the WAV and prints JSON, exit 1 on failed expectation.
3. `make ears` runs `tools/ears/suite.py`: a table of cases (render args, analyzer, judge), coloured PASS/FAIL, non-zero exit on failure. `make ears CASE=s2` runs a prefix (s2, s3, s4, s7 = plan steps). A new check = a new `Case` in suite.py, not a shell line.
4. `make s8` renders the human listening set (`docs/s8-listening.md`).

```sh
make test      # unit tests (ctest)
make ears      # render + measure
build/tidbank_render --out $TMPDIR/x.wav --bpm 120 --div 0.5 --fb 0.5 --dry 0 --secs 6
uv run --locked --script tools/ears/analyze.py taps $TMPDIR/x.wav --expect-ms 250 --tol-ms 0.1
```
`--div` is in quarter notes (0.5 = 1/8, 0.75 = dotted 1/8). Use `--dry 0` so taps are echoes only. Stimulus impulse sits at 0.1 s (`--onset-s` in analyze). Scratch output goes to `$TMPDIR`, never into the repo.

Python deps come from a script header plus a committed lock: `uv run --locked --script tools/ears/x.py` (hashes checked). Change or update a dependency with `uv lock --script tools/ears/x.py [--upgrade]` for all four scripts (suite, analyze, drumloop, normalise) at once, then run the full ears. `make pins` (in CI) fails on anything else. Trap: `uv run --with-requirements` does NOT check hashes, a wrong one is accepted (uv 0.6.17). No venv in the repo.

## Measurements that exist (`tools/ears/analyze.py <cmd> file.wav`)
| cmd | measures | notes |
|---|---|---|
| `taps` | echo times (ms), per-tap level (dB), error vs `--expect-ms` multiples | sub-sample via parabolic peak |
| `spectrum` | per-tap spectral SHAPE vs tap 1, dB at `--freqs`, normalised to 1 kHz | isolates filtering (HPF/LPF/age) from level; 1/3-octave band power; Hann window centred on the tap, 20 ms |
| `wowflutter` | tone in: wow (0.5-6 Hz) and flutter (6-200 Hz) rms/pp %, dominant modulation Hz | unweighted (no IEC/DIN weighting yet); needs a steady tone >= 5 s, `--trim-s 0.5` |
| `glide` | tone in: frequency ratio over time, min/max/final, `settle_s`, `area_s` | `area_s` = sum(ratio-1)dt = old delay - new delay for an echo, for ANY dynamics: the key invariant for tape time changes |
| `thd` | sine in: THD dB and H2..H5 | Blackman-Harris, +-4 bins per harmonic; `--max-thd-db` to assert |
| `health` | NaN/Inf, float32 subnormals, DC, peak | exit 1 on non-finite or subnormals |
| `step` | probe step response: overshoot, 99 % settle | for `--probe v` |
| `stats` | min/max/mean, `--windows a:b,c:d` peak per window | probes; bounded-loop checks |
| `alias` | two-tone (pilot + probe) through a glide: inharmonic power re probe, per frame | assert the MEDIAN: on an ideal resampler the max reads -49.7 dB at glide onset (analyzer floor) |
| `tf` | `--ref in.wav`: Welch transfer function after aligning by the cross-correlation delay; dB re 200 Hz, -3 dB point, coherence | LTI paths only (BBD with `comp=0`); stimulus from `tidbank_render --write-in` |
| `level` | RMS and peak dB in `--start-s/--end-s` | steady-state levels (compander tracking, noise) |
| `repeat` | `--period-ms T`: tap k+1 / tap k spectrum ratio at exact `--freqs` | per-pass loop response; exact where 1/3-octave `spectrum` smears a peak |
| `bursts` | RMS of tone bursts at onset + k*period, `step_db` | per-repeat level |
| `click` | high-pass (default 2.5 kHz) then peak +-50 ms around `--t-step`, with a baseline | switch clicks |
All windowed analyzers take `--start-s/--end-s`; `thd` reports `inharmonic_db` (7-term window, +-8 bins); `taps` takes `--floor-db/--min-gap-ms` (filter ringing otherwise counts as taps); `spectrum --win-ms`; `glide --f0`.

`selftest.py` proves each one on synthetic signals with known answers (sub-sample taps, one-pole filter chains, FM wow/flutter, first-order glide, Gaussian pitch bump, harmonic levels, NaN/subnormal). Run it first: if an engine test fails, the measurement is not the suspect. It already caught one real analyzer bug (a Hann window starting before the tap put the tap on the window skirt, biasing per-tap spectra by ~1 dB) and exposed one imprecise test expectation (expected values must use the same 1/3-octave band averaging as the analyzer).

Stimuli of `tidbank_render --stim`: `impulse` (default, at 0.1 s), `clicks` (one per beat at `--bpm`), `tone` (`--freq`, `--level`, 10 ms fades), `twotone` (`--freq2 --level2`), `burst` (20 ms 1 kHz, one per beat), `noise` (deterministic). Or `--in x.wav` (`tools/ears/drumloop.py` makes a synthetic drum loop at an exact tempo). Engine: `--engine naive|tape`, `--params k=v,...`, `--sched "t:k=v;t:k=v"`, `--probe v|tau1..3|tauerr1..3|clamped`.

## Still to write (add here when done)
- Noise-floor/hiss command, IEC/DIN-weighted wow/flutter (not needed while D9 keeps those out of TidBank).
- `inst_freq` (glide) is biased by harmonics: a saturated 0.5-level tone read 0.99899 steady. Band-limit it around f0 before glide tests at high levels; until then glide cases use level 0.05.

## Traps
- Hardware takes (skill `matriarch-bench`) add their own: find events in the recorded reference, not the schedule;
  a periodic test tone is ambiguous by a period (and by half a period with |correlation|), so use noise or decide
  polarity first; check an unconnected input before calling a spectral line the device's.
- Engine API reads before it pushes: a feedback loop's delay is `DelayLine::read(d)` + 1 sample. This was a real bug the ears caught (every tap one sample late, 0.0208 ms at 48 kHz). Subtract 1 when reading before pushing.
- `--in` WAV reader supports PCM16/24 and float32, first channel only. The renderer is mono for now.
- The ears prove timing and level. They do not prove "sounds good". Final taste check is the human on a real loop.

## Debug probe (`dsp/DebugProbe.h`)
Compiled only with `TIDBANK_DEBUG_PROBE` (CMake option, OFF by default; the plugin and `tidbank_render` then contain no probe code). `build/tidbank_render_probe` is the same renderer with it compiled in, always built, so the ears test it without a reconfigure.
`tidbank_render_probe --engine tape|bbd ... --probe-csv out.csv --probe-interval 1 --probe-offset-s 0.1 [--probe-block 512]`: every interval an input sample n0 is marked; one CSV row per mark and head (tape head 1..3, BBD head 0) with n0, n_out (sub-sample), measured and computed delay (samples, ms), diff, speed (BBD: clock Hz), bpm, block start/length of the input and output, plus `block` and `cpu` rows. Data for collecting, not a speed test (CPU rows are wall-clock). Audio is unchanged (`p_null_*`: same bytes probe on / off / flag unset, and vs a pre-change build when `TIDBANK_BASELINE_RENDER=<render built from origin/bbd-plugin>`).
Cases: `p_tape_const`, `p_tape_glide` (mode 7 = all heads, |measured - computed| <= 1 sample, >= 1 record per head, glide: >= 1 record with speed strictly between the two end speeds; each solo head also checks the echo's energy centroid in the WAV against n_out, 1 sample), `p_bbd`, `p_null_*`. Tape: the head plays input time `n - tau_i(n)` at call n (kReadOffset absorbs read-before-push and the ADAA half sample). BBD: a mark waits for the first clock tick at or after the start of call n0; that tick's cell is read out `cells` ticks later (`out = tick time`).
Trap: measured vs computed is a tautology for the tape on its own (the crossing is found from the engine's own tau); the WAV centroid check is what ties the probe to the audio. Do not drop it.

## BBD
`tidbank_render --engine bbd --profile m8192|d4096|c8192 --params time_ms=..,fb=..,bright=0|1,comp=0|1,noise=0|1,nonlin=0|1`.
`--dump-profile all` prints each profile's facts (with verified/inferred/placeholder) and its designed filter response; `make ears CASE=b` builds b1-b10 from it.
A case in `KNOWN` (suite.py) is a known failure tied to a known open issue; if it starts passing the run fails (`FIXED?`) so the entry is removed on purpose.
