---
name: reels-plugin
description: Reels, the reel-only looper plugin of the TidBank family: what v0 does, the timing model of dsp/Reel.h (bar grid, seams, layers, erase), its tests, build and install. Use before touching dsp/Reel.h, reels/, or when the listener reports looper behaviour from a DAW.
---

# Reels (v0, 2026-11-17)

**Shape (decided 2026-11-17):** a plugin with the reel only, no echo inside. On one DAW track it goes Reels -> TidBank (-> a separate tape-ageing plugin later in the chain). Output = `input_level x in + reel_level x reel`. Codes `Stlu`/`Rels`, bundle `com.stiglau.reels`, product "Reels". Never reuse TidBank's codes (`Tdbk`) or those of any other plugin.

**v0:**
- 8 parameters in host order: Rec (state), Undo, Redo, Clear (fire on 0 -> 1; the editor buttons send one-block pulses), Bars (1/4/16, the next layer only; never 2 or 8, decided by the maintainer), Reel level, Input level, Nudge.
- **Erase was taken out 2026-11-17** after trying it in the DAW (it did not erase anything; it only muted layers *below* a new take). The idea instead is a gesture that ducks the whole output, all layers, with a drag-down depth and soft attack/release. It is a POSSIBLE feature, back on the drawing board.
- **Nudge (2026-11-18):** a hand on the reel, -1..+1 = speed 0x..2x. The hand follows in 60 ms, and on release the motor slews back to 1x in 400 ms (99 %, critically damped).
  - Only the speed returns, never the phase (grid option (b)): the reel keeps its offset through stop/start and locate. Clear resets it.
  - Rec while nudged records at reel speed (tape-like).
  - Playback EQ: `y = x - (1 - v) LP50(x)`, a NAB 3180 us EQ run at speed v. The lows fade as the reel slows, and a stopped reel goes silent. Exactly `y = x` at 1x.
  - Editor: a spring-loaded strip. Drag from anywhere, release springs back to 0. The status line shows the speed and the ms off the grid.
- No Print, no varispeed, no `rec_offset`, no reel filters. Layer audio is not saved with the project (plan Q8 (a)).
- The plan is `docs/plan-looper.md`; its status block at the top says what v0 changed.

## Model (`dsp/Reel.h`, header-only, JUCE-free)
- **Position:** host sample `S = llround(ppq x 60 / bpm x sr)`. Layer pass k starts at `round(k L)`, with `L = 4 x bars x 60 / bpmRec x sr` (fractional), so there is no drift.
- **Reel time (nudge):** `S + nOff + frac`. A layer reads and writes at `cellPos(S + nOff) + frac`, wrapped at L. While `frac != 0` every read is linear-interpolated, which is about 1.3 dB duller on noise. Once the motor rests, `frac` glides to a whole sample at <= 1/256 speed change, and playback is raw again.
- **Cells:** a layer holds `len = ceil(L)` cells. Positions are raw except in the last 64 samples of a pass, where they ramp to reach `L` at the next pass start. Raw cells play back bit-exact; with an integer L there is no ramp at all.
- **Layers:**
  - a stack in one pool of 256 s stereo float32 (98 MB at 48 kHz), allocated in `prepare()` only;
  - at most 16 layers; a take that does not fit is refused (flag `full`), never truncated;
  - Undo/Redo move `heard` with 5 ms fades; a new take drops the undone layers above and reuses their memory.
- **Rec:** opens at the current cell (5 ms fade in) while the host plays. Rec pressed while stopped = armed: the take starts on the first sample the host reports playing (after the host's count-in, if it reports the count-in as not playing: unverified). Held over a pass, it overdubs into the same layer.
- **Transport:**
  - stop: 10 ms fade out, then halt; a recording is closed;
  - start: resync from ppq, then fade in;
  - locate: fade out, jump, fade in (`splices` + 1);
  - a host loop that lands on the same place in every layer is followed without a fade;
  - a tempo change mutes the layers recorded at another tempo (`tempoMuted`) and closes a recording.
- **Refused or cut take** (full, Undo during a take, Clear): `recBlocked` stays until Rec is released, and the editor's timer releases the Rec parameter so its light matches.

## Tests
```sh
make test                                   # includes tidbank_reel_tests (l0/l1/l3 cases of plan section 5)
make plugin-check JUCE_SRC=<juce-src>       # also runs reels_plugin_tests; never installs
build/plugin-check/reels_plugin_tests_artefacts/Release/reels_plugin_tests out.png   # + offscreen editor PNG
```
What `reel_tests` covers:
- bit-exact null at 120 BPM / 48 kHz;
- no drift over 64 passes at 97 BPM / 44.1 kHz and 140 BPM / 48 kHz (the impulse lands on `round(kL) + 5000`, within 0.5 sample of `kL`);
- seam sine residual below -60 dBFS (-78 measured; -46 with the ramp removed, so the test catches a broken seam);
- overdub +6.02 dB, undo/redo, a new take after undo;
- mixed 1-bar and 4-bar layers;
- stop/start, locate, host loop;
- the same output at block sizes 1, 64 and 1000;
- full pool, tempo mute, zero allocations after `prepare()`;
- nudge:
  - hand at 2x in 60 ms, motor within 1 % in 400 ms;
  - raw playback at the kept offset after rest and after stop/start; Clear resets it;
  - brake to silence (< -60 dB) and back within 1 dB;
  - pitch 1.5 kHz at +0.5;
  - half-speed recording plays back at 2 kHz;
  - block-size invariance with nudges.

## Traps
- `make plugin` builds AND installs both TidBank and Reels from this branch. That replaces whatever TidBank test build is installed. To install only Reels, copy `build/plugin-check/Reels_artefacts/Release/{VST3/Reels.vst3, AU/Reels.component}` into `~/Library/Audio/Plug-Ins/{VST3,Components}`, then run `auval -v aufx Rels Stlu`.
- Memory: every instance takes the full pool at `prepareToPlay` (98 MB at 48 kHz). pluginval strictness 5 passes with it.
- Overdub latency: a monitored live input reaches the plugin late by the interface round trip, so overdubs land late until `rec_offset` exists (plan section 8).
