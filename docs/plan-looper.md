# Plan: L1 tape looper / rhythm composer

**Status 2026-11-17: Reels v0 built (branch `reels-v0`).** The shape was re-decided after the routing and print-head discussion of 2026-11-16/17 (see `reflections-routing.md`):
- **Decided 2026-11-17:** Reels is a reel-only plugin with no echo inside. Live chains Reels -> TidBank (-> a tape-ageing plugin) on one track, and Reels has its own short parameter list. This replaces the 2026-11-16 decision ("contains the same echo engines") and withdraws the copied-banks decision of the same date. Section 2.1's echo box, `reel_send`, the send filter and section 4's banks 4-5 are therefore superseded. Reels' output is `input_level x in + reel_level x reel`.
- **Decided 2026-11-17:** v0 = L0 + L1 plus mixed lengths from L3, and a plain plugin (Rec, Undo, Redo, Clear, Bars, Reel level, Input level). No Print, no varispeed: a layer recorded at another tempo is muted, and the editor says so.
- **Erase (L3, Q5) was taken out 2026-11-17** after it was tried in Ableton Live. It is back on the drawing board as a possible feature: a gesture that ducks the whole output, all layers, with a drag-down depth and soft edges.
- **Where it is:** core `dsp/Reel.h`; tests `tests/reel_tests.cpp` (the l0/l1/l3 cases of section 5, measured in C++ instead of through the ears renderer); plugin `reels/`; plugin checks `tests/reels_plugin_tests.cpp`; skill `reels-plugin`.
- **Seam, as built (differs from 2.2):** a pass is `len = ceil(L)` or `len - 1` samples. Read and write positions are raw except in the last 64 samples, where they ramp so the position reaches exactly `L` at the next pass start. The content stays continuous at every seam: residual -78 dBFS at 97 BPM / 44.1 kHz, and -46 dBFS with the ramp removed.
- **Nudge (2026-11-18):** a hand on the reel (0x..2x), and the motor slews it back. Only the speed returns, never the grid phase; Clear resets it. A playback-EQ low shelf runs at reel speed. Open questions were decided as recommended (2026-11-18).
- **Still open:** Print (needs a print head, a second plugin sharing the reel in-process), varispeed (L4), `rec_offset`, the reel HPF/LPF and saving audio (Q8 (b)).

Status 2026-11-16 (original draft; the status block above supersedes parts of it). Source: `whitepaper.md` section 7 (sketch, 2026-11-15), designed within D1', D2', d11-d21. Template: `plan-tape-engine.md`. Thresholds and estimates are the author's unless a source is named; they are chosen to catch bugs and may be loosened only with a reason written here. Section "Decisions" lists what the plan assumed; the plan is written for the recommendation in each.

## 1. Goal and scope

A reel that loops in time with the host, that the player overdubs in layers, with the existing echo engine (tape or BBD) treating the reel, and a Print that records the echo back onto the reel.

**v1 is:**
- layers of 1, 4 or 16 bars (4/4), each locked to the host bar grid, never drifting;
- Rec (live input into a new layer), Print (one loop pass of echo output into a new layer), Erase on/off (replace vs add, as mute-not-delete), Undo/Redo, Clear;
- reel send to the echo through an HPF/LPF; reel level; print level;
- tape-like tempo change: a layer recorded at another tempo plays varispeed (pitch follows);
- all memory from one pool allocated in `prepare()`.

**v1 is not:** flatten, held sound-on-sound printing (multi-pass into one layer), saving the audio with the project, per-layer gain/pan, time signatures other than 4/4, time-stretch, reel wow/wear (`X mod L`, whitepaper section 6), ping-pong, an editor beyond plain controls.

## 2. Model

### 2.1 Signal path (per sample, per channel)
```
in ─┬──────────────────────────────────────────────► dry_level ─────────┐
    │                                                                   (+)─► output ─► out
    ├─► Rec ─► new layer                 reel mix ──► reel_level ───────┤
    │                                      │                            │
    └──────────────► (+) ◄── reel_send ◄── HPF/LPF                      │
                      │                                                 │
                      ▼                                                 │
                 echo engine (TapeEngine or BbdEngine, unchanged) ─► wet ┴─► echo_level
                                                                    │
                              Print ─► new layer ◄── print_level ◄──┘
```
Order inside one sample: `reel.read()` -> send filter -> `engine.process(in + send)` -> `reel.write(in, wet)`. Read before write, so a printed sample lands on the cell it was heard at (no extra sample; compare the read-before-push trap in `tidbank-tape-engine`).

With an empty reel the path is today's TidBank: same bytes (case `l0_idle_hash`).

### 2.2 The reel and the two transports
- The echo keeps its `TapeTransport` untouched: Division, Nudge, Inertia, Spacing act on the echo heads only.
- The reel is a second transport, with the host as its motor. Its position comes from the host's ppq, not from Division/Nudge. **No inertia and no nudge on the reel:** both mean drifting against the DAW, which the sketch rules out. What the reel shares with D1' is the idea: position is the integral of speed, and pitch is the speed ratio.
- A layer stores: pool offset, length in cells, length in beats (`4 * bars`), the tempo it was recorded at (`bpm_rec`), written span (start, length), erase flag.
- Layer speed `v_j = bpm_host / bpm_rec_j`. Exact loop length `L_exact = beats * 60 / bpm_rec * sr` (a double, usually fractional: 1 bar at 97 BPM, 44.1 kHz = 109113.40 samples).
- `v = 1` (the normal case): cells are read and written on the host sample grid, raw, no interpolation. A recorded layer plays back bit-exact.
- **Seam (the whitepaper's "re-align each pass").** Pass k starts at host sample `round(k * L_exact)`, so a pass is `floor(L_exact)` or `floor(L_exact)+1` samples. Loop start is within 0.5 sample of the host bar on every pass and the error never accumulates. The one-sample slip is spread over the last 64 cells of the pass (linear interpolation, on read and on write) so it does not click. Proposed mechanism; the L1 implementer may pick another if `l1_seam_click` and `l1_nodrift` pass.
- **Timing floor of the looper is +-1 sample** (0.021 ms at 48 kHz), not the engine's 0.01 ms: the host grid and a fractional loop are incommensurate without resampling the writes, and resampled writes would cost HF on every print generation.
- The core integrates its own position per sample and compares with the host ppq once per block. Phase is compared modulo the layer length, so a host loop of 4 bars around a 1-bar layer is not a jump.

### 2.3 Layers, Rec, Print, Erase, Undo
- Layers are a stack in one pool (stack allocator: a layer takes `length` cells at the pool top). `count` layers exist, the lowest `heard` of them sound.
- **Undo** = `heard - 1`, **Redo** = `heard + 1`, both with a 5 ms gain fade. Nothing is deleted. A new recording drops the layers above `heard` and reuses their memory (ordinary undo-stack rule).
- **Rec** (state parameter, 1 = recording): starts at once at the current cell, 5 ms fade in/out at the punch points, source = live input, raw. Held over more than one pass it adds into the same layer.
- **Print** (trigger): arms; at the next seam records exactly one pass of `print_level * wet` into a new layer, then stops by itself. In steady state the wet signal is periodic with the loop, so one pass already contains the tails that wrap over the loop end.
- **Why one pass:** during the pass nothing that is being written is read back, so printing has no feedback loop of its own and cannot run away. Each further generation is a new press. Gain of a generation is `print_level` x the echo's gain, which for periodic material can reach `heads / (1 - intensity)` (inferred from the head sum; output is not normalised, only the feedback is), so `print_level < 1` alone does not make repeated printing quieter. Held sound-on-sound with pull-back is a later step.
- **Erase** (state parameter; "erase head down"): a layer recorded with Erase on mutes everything below it **inside its written span**, with 5 ms edges. Erase off = add. Mixing rule, top layer first: `out += g * layer_j; if erase_j: g *= 1 - cover_j`. Undo of that layer brings the old sound back. A full-pass Print with Erase on leaves only the echoes.
- **Rendered mix** = summed per sample from the layers (at most 16 float reads per channel at `v = 1`). No cached mix buffer: nothing to re-render on undo, no extra memory.
- Unwritten cells are never read (the written span says so), so opening a layer needs no memset on the audio thread.
- **Bars** sets the length of the *next* layer. Existing layers keep theirs and are read at `position mod own length`, so a 1-bar layer tiles under a 4-bar or 16-bar one for free, and changing Bars never touches audio.

### 2.4 Tempo, transport, loop length
| Event | Behaviour |
|---|---|
| Host tempo changes | Each layer plays at `v_j = bpm / bpm_rec_j` through the 32-tap sinc (pitch follows, like tape). Position still locked to ppq. New layers record at `v = 1` at the new tempo. |
| Tempo changes while recording | The recording is closed with its fade (punch out). An armed Print waits until the tempo is steady for one block. |
| `v_j` outside 0.5..2 | That layer fades out, `reelClamped` flag for the UI (as `isClamped()`). |
| Transport stop | Reel output fades 10 ms and the reel halts; a recording is closed. The echo keeps ringing. |
| Transport start | Position from ppq, 10 ms fade in. |
| Locate / host loop with a phase jump > 2 samples | Splice: 10 ms crossfade from old to new position; a recording is closed. |
| No playhead (standalone, ears) | Internal ppq from the Tempo parameter, always playing. |
| Bars changed | Next layer only (2.3). |
| Sample rate change (`prepare`) | Pool reallocated, reel empty. |
| Panic | Echo erased as today; recording stops; **reel kept**. Clear is the only thing that empties the reel. |

## 3. Real-time design

- `Reel::prepare(sr, poolSeconds, channels)` is the only allocation: one `float` buffer of `poolSeconds * sr` per channel, a 16-entry layer table, the sinc table. Rec, Print, Undo, Clear, Bars are index arithmetic on the audio thread: no allocation, no memset, no memmove.
- Float32 storage. 16-bit would halve the memory but adds quantisation on every print generation and breaks the bit-exact null tests.
- Pool bytes = `poolSeconds * sr * 4 * channels`. Layer seconds = `bars * 240 / bpm`.

| Pool (stereo) | 44.1 kHz | 48 kHz | 96 kHz |
|---|---|---|---|
| 128 s | 45 MB | 49 MB | 98 MB |
| **256 s (proposed)** | 90 MB | 98 MB | 197 MB |
| 512 s | 181 MB | 197 MB | 393 MB |

| Layer | Seconds | Mono layer at 44.1 / 48 / 96 kHz | Layers in a 256 s pool (cap 16) |
|---|---|---|---|
| 1 bar, 120 BPM | 2 | 0.35 / 0.38 / 0.77 MB | 16 |
| 4 bars, 120 BPM | 8 | 1.4 / 1.5 / 3.1 MB | 16 |
| 16 bars, 120 BPM | 32 | 5.6 / 6.1 / 12.3 MB | 8 |
| 16 bars, 60 BPM | 64 | 11.3 / 12.3 / 24.6 MB | 4 |
| 16 bars, 40 BPM | 96 | 16.9 / 18.4 / 36.9 MB | 2 |

  MB = 10^6 bytes. For comparison, fixed slots (8 layers x 64 s stereo) cost the 512 s row whatever is recorded. A layer that does not fit is refused (`reelFull` flag), never truncated.
- CPU (estimate, to be measured in `l0_cpu`, `l4_cpu`): `v = 1` is a handful of adds; varispeed is one 32-tap sinc per layer per channel, at 16 layers about 5x the three heads that S7 measured at 0.71 % per mono engine.
- Threads: parameters as today (APVTS atomics copied per block). Flags to the UI as atomics (`layers`, `heard`, `recording`, `reelFull`, `reelClamped`).
- **State saved (v1):** parameters only. Layer audio and the layer table are not saved; a reopened project has an empty reel. See decision Q8.
- Latency stays 0. The send filter is minimum-phase IIR (same TPT SVF as the loop filters).

## 4. Parameters (d20 banks)

Banks 1 and 2 do not change: same ids, same order. Bank 3 holds Panic, Engine, BBD profile, BBD bright after PR #2, with four free slots that BBD work (d18 selector) will want. The looper gets two new banks. No mode switch: the echo knobs are needed while looping (riding Intensity during a Print), so banks 1-2 must not change meaning.

| Slot | Bank 4: looper performance | Bank 5: looper setup |
|---|---|---|
| 1 | `rec` Rec (bool, state) | `bars` Bars (1 / 4 / 16, default 4) |
| 2 | `print` Print (bool, fires on 0 -> 1, like Panic) | `reel_hpf` Reel HPF 20-2000 Hz log, 20 |
| 3 | `erase` Erase (bool; off = add) | `reel_lpf` Reel LPF 1-20 kHz log, 20000 |
| 4 | `undo` Undo (fires on 0 -> 1) | `rec_offset` Rec offset 0-50 ms, 0 (writes land earlier; for monitoring latency) |
| 5 | `redo` Redo (fires on 0 -> 1) | `reel_clear` Clear (fires on 0 -> 1) |
| 6 | `print_level` Print level 0-1, 0.7 free (was `looper_on`, dropped: Q1 = (b)) |
| 7 | `reel_level` Reel level 0-1, 1 | free |
| 8 | `reel_send` Reel send 0-1, 1 | free |

Decided (Q12, 2026-11-16; withdrawn by the reel-only decision of 2026-11-17): the looper plugin copies TidBank's banks 1-3 exactly, with bank 3 padded to 8 by four inert reserved parameters (until BBD uses them), and appends the looper banks 4-5. `plugin-test` asserts that the looper plugin's parameter list starts with TidBank's list exactly. TidBank itself has no looper parameters and no `looper_on` (Q1 = (b)). Ids are stable strings, so a later reorder (if L6 in Live says so) does not break saved state.

## 5. Ears, test-first

Written before the code, as the B-cases were (`bbd_cases()`): each step adds `looper_cases()` entries to `tools/ears/suite.py` and watches them fail first.

**Additions (L0):**
- `tidbank_render --looper 1 --block N` (host simulation: ppq integrated from `--bpm`, parameters applied per block; default 64). `--params`/`--sched` keys: `bars, rec, print, erase, undo, redo, clear, print_level, reel_level, reel_send, reel_hpf, reel_lpf, rec_offset`, and host events `bpm, playing, locate` (ppq). `--tap out|reel|send|wet` chooses what is written. `--probe layers|heard|recording|splices|reelclamped`.
- `analyze.py null out.wav --ref ref.wav --shift-samples N --start-s --end-s` -> `resid_db` (residual power re reference; `-inf` reported as -999). In `selftest.py` in the same change (identical file = -999; known -60 dB added tone = -60 +-0.1).
- `tests/looper_tests.cpp`: a counting global `operator new`; zero allocations between `prepare()` and the end of 10 s of processing with every trigger fired.

Base setting unless stated: 48 kHz, 120 BPM, 1 bar = 96000 samples exactly, echo off (`--tap reel`).

| Case | Stimulus and actions | Assert |
|---|---|---|
| `l0_idle_hash` | drum loop, `--looper 1`, nothing recorded | output file hash equals the same render without `--looper` |
| `l0_period` | impulse at 0.1 s, rec 0-2 s, 10 s | `taps --expect-ms 2000 --tol-ms 0.001`: 4 taps |
| `l0_null` | noise, rec 0-2 s | `null` pass 2 vs input, shift 96000: -999 (bit-exact) |
| `l0_level` | burst 0.25, rec one pass | `bursts` step over passes 2-5: 0.00 +-0.01 dB (a loop does not decay) |
| `l0_overdub` | 500 Hz tone 0.1 (1000 cycles per loop), rec pass 1 and pass 3 | level pass 5 minus pass 2 = +6.02 +-0.05 dB; probe `layers` = 2 |
| `l0_undo_redo` | as above, undo at pass 6, redo at pass 8, undo + new rec at pass 10 | pass 7 = pass 2 level +-0.05 dB; pass 9 = +6.02; probe `layers` = 2 at the end (not 3) |
| `l0_punch_click` | 500 Hz at -20 dBFS, rec 2.5-3.0 s | `click` at 4.5 s and 5.0 s < -60 dBFS |
| `l0_full` | 3 s pool, 1-bar layers (2 s each), rec twice | first accepted, second refused: `layers` = 1, `health` ok |
| `l0_cpu` | 8 layers, 60 s | render < 1 % of real time per channel; record the number |
| `l1_nodrift_97_441`, `_140_48` | clicks, 1 bar, 64 passes (L = 109113.40 and 82285.71) | every pass start within 1 sample of `k * L_exact`; a plain integer loop is 25.7 samples late at pass 64 in the first |
| `l1_seam_click` | tone at `1237 / T_loop` Hz (whole cycles per loop; a 500 Hz tone would click by itself), -20 dBFS, rec 2 passes, play 16, 97 BPM 44.1 kHz | `click` at every seam < -60 dBFS |
| `l1_locate` | `locate` mid-pass | `click` < -60 dBFS; next taps on the new phase +-1 sample; `splices` = 1 |
| `l1_host_loop` | host jumps 4 bars -> 0, 1-bar layer | `splices` = 0; hash equals the run without the jump |
| `l1_stop_start` | `playing=0`, later `playing=1` | reel `level` 20 ms after stop < -120 dB; wet tail still present; `click` < -60 at both; taps on grid after start |
| `l1_block_invariant` | `--block 1`, 64, 1000 | three identical hashes |
| `l2_echo_of_reel` | reel impulse, echo 1/8, head 1 | wet taps 250 ms after the reel impulse, +-0.01 ms |
| `l2_send_filter` | reel noise, HPF 500, LPF 2000, `--tap send` vs `--tap reel` | `tf`: both -3 dB points within 3 % |
| `l2_print_timing` | reel = 1 impulse per bar, intensity 0.5, drive -12, print after 8 passes, then send 0 | printed taps at k x 250 ms, error 0 samples (+-0.001 ms); steps -6.02 +-0.3 dB |
| `l2_print_null` | print_level 1; compare `--tap out` of the pass before the print with the reel alone two passes later (send 0) | `null` < -100 dB: after a print the reel alone sounds like reel + echo did |
| `l2_print_one_pass` | print armed mid-pass | `recording` rises at the seam sample and falls one pass later, exactly |
| `l2_tail_wrap` | reel impulse at beat 3.75 | the printed layer has the wrapped taps at the loop start, level +-0.3 dB of predicted |
| `l2_generations` | mode 7, intensity 0.45, print_level 0.7, three prints, then 10 passes without | `health` ok; peak per pass constant +-0.01 dB over the last 10 passes (no self-feedback) |
| `l3_add_keeps` | tone A layer, rec silence beat 2-3, erase 0 | `null` vs before < -120 dB |
| `l3_erase_region` | same, erase 1 | span RMS < -100 dB; outside `null` < -120 dB; `click` at both edges < -60; after undo `null` < -120 dB everywhere |
| `l3_erase_full` | tone A, full-pass print of tone B with erase 1 | A < -100 dB re before; undo: A back +-0.01 dB, B < -100 dB |
| `l3_mixed_lengths` | 1-bar clicks under a 4-bar impulse | 4 clicks per 1 impulse, all +-1 sample |
| `l4_pitch` | 1 kHz layer at 120, host to 126 | `glide` final ratio 1.0500 +-0.1 %; loop start on the host bar +-0.05 ms |
| `l4_return` | back to 120 | `null` vs a pass before the change < -100 dB |
| `l4_alias` | pilot + 5 kHz probe, ratio 1.25 and 0.8 | `alias` median < -60 dB (the S2 number) |
| `l4_ramp` | host tempo 120 -> 140 over 4 s | `splices` = 0; `click` < -60; phase error < 2 samples at the end |
| `l4_rec_punchout` | tempo step while rec = 1 | `recording` falls within one block; `health` ok |
| `l4_clamp` | 120 -> 300 | `reelclamped` = 1; reel silent; `click` < -60 |
| `l4_cpu` | 8 layers at v = 1.05, 60 s | < 5 % of real time per channel (S7's budget); record the number |

## 6. Steps

**L0 Reel core, integer loops** (Sonnet; about 250 lines `dsp/Reel.h`, 150 lines renderer + suite + analyzer). No host lock, no echo, no Erase, `v = 1` only, loop length `llround(L_exact)`, position from an internal sample counter starting at a bar. Build exactly this:
```cpp
// dsp/Reel.h  (namespace tidbank, header-only, JUCE-free, no allocation after prepare())
struct ReelParams { int bars = 4, rec = 0, print = 0, erase = 0, undo = 0, redo = 0, clear = 0;
                    float print_level = 0.7f, rec_offset_ms = 0.0f; };
class Reel {
public:
    static constexpr int kMaxLayers = 16, kMaxCh = 2;
    void prepare(double sampleRate, double poolSeconds, int channels);  // the only allocation
    void reset();                              // audio-thread safe: closes a recording, keeps the layers
    void setHost(double bpm);                  // per block. L1 adds ppq and playing
    void setParams(const ReelParams&);         // per block; detects 0 -> 1 on print, undo, redo, clear and changes of rec
    void read(float* out);                     // advance one sample; out[ch] = sum of the heard layers
    void write(const float* in, const float* wet);  // after the echo; L0 uses `in` only
    double probe(int which) const;             // 0 layers, 1 heard, 2 recording, 3 splices, 4 reelclamped
};
```
  - Layer = `{ size_t offset, len; double beats, bpmRec; size_t spanStart, spanLen; bool erase; double gain, gainTarget; }`.
  - Rec 0 -> 1: if `heard < count`, set `count = heard` and the pool top to that layer's offset. If `count == kMaxLayers` or the pool has fewer than `len` cells free: set `reelFull`, do nothing. Else open layer `count` at the pool top, `spanStart` = current cell, `spanLen = 0`, then `count++`, `heard = count`.
  - `write`: while recording, `cell = in * fade` (5 ms linear in; on rec 1 -> 0 keep writing 5 ms with fade out, then close); once `spanLen == len`, `cell += in * fade`.
  - `read`: for each heard layer, if the current cell is inside the written span, add `gain * cell`. `gain` moves to `gainTarget` linearly in 5 ms (undo/redo).
  - Clear: `count = heard = 0`, pool top 0.
  - Accept: `l0_*` green, `make test` green including the allocation counter, all earlier ears unchanged.

**L1 Host lock and the seam** (Opus; a subtle error here passes casual tests, as the quasi-static flaw did). `setHost(ppq, bpm, playing)`, fractional `L_exact`, seam slip, stop/start, locate/splice, block invariance. Accept: `l1_*`. If `l1_seam_click` fails twice, a stronger review model gets the failing numbers.

**L2 Echo box and Print** (Sonnet, Opus reviews the read/write order). Send filter (move the TPT `Svf` out of `FeedbackNetwork.h` into `dsp/Svf.h`; earlier ears must stay byte-identical), the composition in `render.cpp`, Print. Works with tape and BBD because it only sees `wet`. Accept: `l2_*`.

**L3 Erase spans and mixed lengths** (Sonnet). Accept: `l3_*`.

**L4 Varispeed** (Opus). Sinc read at a fractional position with wrap, switch between the integer and the sinc read without a click, servo to the host ppq, clamp. Accept: `l4_*`.

**L5 Plugin** (Sonnet). Wrapper per Q1, banks per section 4, flags to a plain UI (layer count, heard, recording, full, clamped, tempo of the top layer). `make plugin-test`: bank 1-2 id list unchanged, empty reel = dry/echo path bit-identical to before, Panic keeps the reel, state round trip, pool size logged. `make pluginval` strictness 5.

**L6 Human gate** (a human player, Ableton Live + Ableton Move). A listening/playing checklist like `s8-listening.md`: overdub feel and `rec_offset`, Print level and generations, Erase on vs off ("which one feels like tape"), tempo change, 1/4/16 mixing, whether the seam is ever heard.

Later, not planned here: flatten, held sound-on-sound with pull-back, audio persistence, per-layer level, reel wear locked to `X mod L`.

## 7. Delegation
As `plan-tape-engine.md`: mechanical steps to Sonnet with this file plus skills `tidbank-ears`, `tidbank-rt`; L1 and L4 to Opus; a stronger review model only on a numeric failure after two attempts or for one review of the L1 seam design before L4 builds on it. Agent-checked claims are re-run by a second agent for QC as usual.

## 8. Risks
- **Seam slip.** Not built anywhere yet; the 64-cell spread is a proposal. If it cannot pass -60 dBFS, the fallback is resampled writes, which costs HF per print generation.
- **Host ppq.** Only Live with Move is a tested host. Tempo automation, Live's loop brace and delay compensation may give ppq that jumps or lags; L1's splice threshold (2 samples) may need tuning. Unknown until L5.
- **Overdub latency.** A monitored live input reaches the plugin late by the interface round trip (understanding of Live, not measured), so overdubs land late. `rec_offset` is the answer; its right value is found at L6.
- **Memory.** 90-200 MB per instance for the proposed pool, more with several instances.
- **No audio in the saved state** (v1): a composed reel is lost on reload unless it was recorded to a Live clip.
- **Sustained varispeed above about 1.1x** folds content near Nyquist (the sinc cutoff is fixed; whitepaper section 4 accepts this for short glides only). `l4_alias` measures it.
- **Print level.** Structurally bounded, but three generations in mode 7 can be loud; there is no limiter on the reel's dry path.
- **Two wrappers** (Q1 = (b), own plugin) can drift apart in bank order; `plugin-test` must assert both.
- **Scope creep.** Each "later" item is cheap alone; together they turn a looper into a DAW.

## 9. Decisions

| # | Question | Options | Recommendation |
|---|---|---|---|
| Q1 | Where does the looper live? | (a) inside TidBank, behind `looper_on`; (b) a second plugin target in this repo that contains the reel and the same echo engines; (c) a looper-only plugin with the echo as a separate device | **DECIDED (b)** 2026-11-16 (later replaced by the reel-only plugin, 2026-11-17). Context: the looper is a separate feature with its own UI panel, and what it has in common with the echo is mainly the tape idea, not a functional requirement on the code. Original reasoning: (c) cannot print: a plugin cannot reach another plugin's output without host feedback routing. (a) puts 100-200 MB, audio-content state and 16 parameters into the echo that is meant to stay simple (D9 scope rule). (b) keeps one codebase (the spirit of d16), costs one more wrapper because `dsp/` is header-only, and keeps banks 1-2 identical so Move muscle memory carries over. |
| Q2 | Host tempo changes with audio on the reel | (a) varispeed per layer, pitch follows; (b) reel refuses to play until cleared; (c) time-stretch | **(a)**: it is what a tape does and what D1' already does for the heads. (c) is a different product. |
| Q3 | Host transport stopped | (a) reel stops, echo rings out; (b) reel free-runs | **(a)**: a bar-locked loop has no position without a running bar. |
| Q4 | Print | (a) one pass per press, new layer; (b) held, sound-on-sound into one layer with pull-back | **(a) for v1**, (b) later: (a) cannot run away and every generation is undoable. |
| Q5 | Erase (replace) | (a) mutes what is under the recorded span only (punch-in, like tape); (b) mutes whole layers | **(a)**: superset of (b) for a full pass, small extra cost. To be judged at L6. |
| Q6 | Bars | (a) length of the next layer; 1, 4 and 16-bar layers mix; (b) one global reel length, changing it needs Clear | **(a)**: nearly free with per-layer length, and it is the "rhythm composer". 2 and 8 bars: NO (decided 2026-11-18: the music uses 1, 4 and 16 bars). |
| Q7 | Memory | pool 128 / 256 / 512 s stereo (table in section 3); float32 or 16-bit | **256 s, float32** |
| Q8 | Reel audio in the saved project | (a) not saved in v1; (b) files beside a state reference; (c) inside the plugin state | **(a) now, (b) as the first follow-up.** (c) puts up to 200 MB into every Live save. |
| Q9 | Reel HPF/LPF | (a) on the send to the echo only; (b) on the whole reel output | **(a)**: "shape a source loop without changing it". |
| Q10 | Print with Erase on | (a) literal: the new layer holds the echoes only; (b) bounce: reel + echo into one layer | **(a)**; (b) is flatten, later. |
| Q11 | Panic | (a) echo only, reel kept; (b) clears the reel too | **(a)** |
| Q12 | Bank layout of the looper plugin (section 4) | (1) copy TidBank banks 1-3 exactly, bank 3 padded with four inert reserved slots, looper banks 4-5 appended; (2) looper plugin orders its own banks: looper in 3-4, TidBank bank 3 moves to 5 | **DECIDED (1)** 2026-11-16 (later withdrawn by the reel-only decision of 2026-11-17). Revisit at L6 in Live if the extra page feels slow. |
