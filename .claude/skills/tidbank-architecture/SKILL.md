---
name: tidbank-architecture
description: TidBank (tape/BBD echo plugin) design decisions, layout, build and the order to build things in. Use before adding or changing anything in this repo, before choosing how delay time, sync, feedback, BBD, MIDI or the looper should work.
---

# TidBank architecture

Why and direction: `docs/whitepaper.md` (routing, zero-latency budget, filters, tape/BBD shared frame, effects, looper). Decision table and parameters: `docs/design.md`. Open research: `docs/research/bbd-matriarch.md`. Verification: skill `tidbank-ears`. Audio-thread rules: skill `tidbank-rt`.

**Status:** D1', D2', D8, D9 and the repo move are ACCEPTED by the maintainer; D5, pluginval, D4 and D3 were decided the same day (2026-11-15): see `docs/design.md`. The Matriarch is measured (filters and delay, 2026-11-17; skill `matriarch-bench`); the Volante is not. m8192 uses the measured TIME range and filter corners (decided 2026-11-17/18); Spacing stays a ratio. Items marked VERIFY are unconfirmed. `docs/design.md` is the decision record.

## Shape
- Standalone CMake project. A separate tape-ageing plugin can be chained after TidBank in the DAW. That was the maintainer's first proposal, not a hard rule (2026-11-16); the family split is in `docs/reflections-routing.md`. Reels (the looper) is a second plugin in this repo, reel only, chained before TidBank (skill `reels-plugin`); Filtær is a third (skill `filtaer-plugin`).
- `dsp/` is header-only, JUCE-free, RT-safe C++17. Renderable offline, so tests and ears build in seconds. JUCE only in `plugin/` (skill `tidbank-plugin`), `reels/` (skill `reels-plugin`) and `filtaer/` (skill `filtaer-plugin`).
- One frame (transport, sync, modes, feedback network), swappable medium (tape now, BBD later). Engines share code, not state. No per-sample virtuals: template or `std::variant`.

## Core ideas
- Tape time = speed state `v` with inertia; tape position `X = integral v`; each head's delay solves `X(t) - X(t-tau) = d_i` (exact, D1'). A BBD obeys the same equation with clock ticks as position. Details: `tidbank-tape-engine`.
- Zero plugin latency: the dry path is untouched; wet-path look-ahead is paid from the echo delay (`read(tau - L)`).
- Feedback = head sum / head weight: loop gain <= intensity in every mode. BBD: the same invariant per profile, by `loopNorm()` (`BbdProfile.h`): fb x 1/peak(|AA x RECON|)^2 with the compander (the compander doubles a filter's dB), 1 for m8192 (no peak). Without it d4096/c8192 ran away from Intensity ~0.07. The BBD output hold (sinc(f / f_clk), squared by the compander) makes the real peak lower at long delays (d4096 0.76 x Intensity at 300 ms), so the runaway point moves with the delay (d4096 Intensity 1.01 at 20 ms, 1.31 at 300 ms). Decided 2026-11-18: that is the expected, authentic behaviour (default), Intensity max raised to 1.4 so long delays still run away; `BbdParams::droopComp` / plugin "BBD hold droop" = Compensated undoes the hold per clock (`loopDroopScale`, 64-point table over log clock) for an A/B, runaway at 1.00-1.01 everywhere. The compander's two gain cells stop at the same knee (`floorAvg`, +/-40 dB); with only the compressor capped, m8192 (no filter loss) barely decayed at Intensity 0.95 and settled into a tone at 42 ms. Since then BBD noise passes the expander at the floor gain, so m8192 at Intensity >= ~1.05 self-oscillates from noise with no input.
- Stereo = two mono engines. BBD: Matriarch-style Spacing and Ping-pong (cross-coupled lines). Tape ping-pong later as an X pattern of writer heads (Volante). No wow/flutter/age/reverb in TidBank: a separate tape-ageing plugin sits after it in the DAW (D9), reverb as a separate plugin.
- Nonlinearity at the record head on `in + fb`; HPF -> LPF in feedback before the sum.
- Matriarch delay as measured (2026-11-17, `docs/research/matriarch-measurement.md`): Bright filter -3 dB at 2.31-2.35 kHz over TIME 43.6-202.9 ms and 2.09 kHz at 403.6 ms, so it barely falls with delay over that range (whether it falls above 404 ms is not measured; clock rate open); Dark 1.41 kHz, measured at 203 ms only; SPACING adds milliseconds rather than scaling Time; a TIME change re-pitches the loop by t_old/t_new and the repeats keep it. Decided 2026-11-18: m8192 takes the measured range (Delay 1 to 764 ms via `spacedMaxMs`) and corners; our Spacing stays a ratio.
- BBD = clock-tick-sampled stage ring + fixed AA/recon filters + companding. Bandwidth falling at long delays is the sound: do not track filters to the clock. Profiles M-8192 / D-4096 / C-8192; feedback normalised per profile (`loopNorm`) so Intensity = peak loop gain; the wet path still has its +23.6 dB forward peak (open).
- Safety: Panic (fade + erase). The output breaker was taken out of the plugin 2026-11-17; `dsp/Breaker.h` stays.
- MIDI: none in the plugin (decided 2026-11-16, supersedes the learn idea). The host (Ableton Live) maps controllers; parameter order is the contract: banks of 8, bank 1 = performance.

## Build
```sh
make test     # JUCE-free, seconds
make ears     # offline render + measure (CASE=prefix for a subset)
make s8       # listening set into build/s8/
```
Uses `/opt/homebrew/bin/cmake` (native arm64 on Apple silicon). Plugin build, install traps and identity (never another plugin's codes): skill `tidbank-plugin`.

## Order of work
ears -> TapeTransport + TapeEngine + FeedbackNetwork (done) -> JUCE wrapper + pluginval (done) -> BBD, three profiles, Matriarch-steered (done in the simple path) -> faces per engine, BBD spacing/ping-pong (done) -> editor sections by role -> LFO -> L1 looper (split into small plugins, see `docs/reflections-routing.md`). Never start the editor or wrapper before the dsp core passes ears.

## Delegation
Cheap models (Haiku/Sonnet) for skeletons, research lookups and re-running verification steps; Opus for hard design calls; a top-tier model only when Opus is not enough (one has reviewed D1/D2 and the plan). Hand an agent the questions and the files, not your hypothesis.
