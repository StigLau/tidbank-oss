---
name: filtaer-plugin
description: Filtær, the Matriarch-style two-filter plugin of the TidBank family - what v0 does, the model in dsp/Filtaer.h (ZDF ladder, routings, SPACING, mode duck), which numbers come from the measured Matriarch, its tests, build and install, and what is not in v0. Use before touching dsp/Filtaer.h or filtaer/, and when the listener reports how the filter sounds in a DAW.
---

# Filtær

Plugin "Filtaer" (codes Stlu/Fltr), chained before TidBank in the DAW; the Matriarch's filters also sit before its delay (Matriarch manual p. 34).
Scope decided 2026-11-18: own plugin, one model (ladder), the Matriarch's three routings;
the MS-20 is the next model. Facts and sources: `docs/research/filter-matriarch.md` (section 1), measured numbers:
`docs/research/matriarch-spacing-measurement.md` (Results).

## What v0 does (`dsp/Filtaer.h`)
- **Two ladders**, each a zero-delay-feedback (TPT) chain of four identical one-poles; feedback solved linearly, then
  `u = tanh(x - k y4)`. The stages are linear: per-stage tanh is a later step. HP = binomial tap mix `(1 - LP1)^4 u`.
  Response at resonance 0 is the exact bilinear 4-pole (-12.04 dB at cutoff, LP and HP).
- **Modes, in panel order:** HP/LP Series (VCF 1 HP -> VCF 2 LP, mono), LP/LP Stereo (VCF 1 -> L, VCF 2 -> R; default),
  HP/LP Parallel (VCF 1 HP + VCF 2 LP, mono). Both filters get the mono sum of the input, as the hardware does; dry stays stereo.
- **Cutoff** 20 Hz - 20 kHz, exponential (measured law); clamped to 10 Hz .. 0.45 fs. **Spacing** moves VCF 1 only, in
  octaves, linear, +-5.8 (measured +5.91 / -5.63: the negative side runs 0.17 oct past the hardware); 0 = unison.
- **Resonance** knob 0..1 -> k = 5 x knob: self-oscillation from 0.8 (3 o'clock, Matriarch manual p. 22). A -180 dBFS noise floor in the
  input lets it oscillate from silence, like the hardware. Self-oscillation sits at the cutoff within 0.03 %, peak about 0.21 (0.14 at cutoff 20 kHz).
- **Bass loss, no compensation:** the LP passband loses the plain ladder's 1/(1+k): -10.9 dB at knob 0.5 (k 2.5), -14.0 dB at
  knob 0.8 (k 4). Matriarch 2026-11-19, per filter, 65 Hz, cutoff ~2.4-2.7 kHz: -10.7..-10.9 dB at RES noon, -13.7..-14.9 dB
  at 3 o'clock. Take 0017 (3.3 / 6.2 dB) is an unexplained outlier, dropped 2026-11-20; the old (1+k)^0.7 fit is gone.
- **Drive** 0..24 dB into the tanh, output divided by it: small signals keep their level. At drive 0 the tanh already colours
  loud input: 3rd harmonic of 100 Hz at 0 dBFS -25 dB, at -6 dBFS -35 dB, at -10 dBFS -43 dB.
- **Mix** 0 = dry, bit-exact. Parameters smooth over 20 ms per sample (block size does not change the output). A mode change
  ducks the wet path for 5 ms, switches with cleared filters, and comes back.

## Plugin (`filtaer/`)
Host order (banks of 8): Cutoff, Resonance 1, Resonance 2, Spacing, Mode, Drive, Mix. New parameters go at the end.
Mono in -> both sides; mono out = both filters summed. Zero latency, no oversampling. The status line shows both cutoffs in
use (after SPACING and the clamp) and which filters self-oscillate.

## Tests and build
```sh
make test                              # includes tests/filtaer_tests.cpp (ctest "filtaer")
make plugin-check JUCE_SRC=<juce-src>  # also runs filtaer_plugin_tests; never installs
build/plugin-check/filtaer_plugin_tests_artefacts/Release/filtaer_plugin_tests ui.png   # editor snapshot
```
Each filtaer_tests check failed when its feature was broken (2026-11-18): tanh removed, SPACING
on VCF 2, mode duck removed. 2026-11-20: kResK +-10 % fails test 5's bass-loss bands (and test 2); a broken
negative SPACING clamp fails the clamp test.
`make plugin` builds and installs TidBank and Reels too, replacing whatever test builds are already installed. To install only
Filtær, copy `build/plugin-check/Filtaer_artefacts/Release/{VST3/Filtaer.vst3, AU/Filtaer.component}` into
`~/Library/Audio/Plug-Ins/{VST3,Components}`, then `auval -v aufx Fltr Stlu` and pluginval strictness 5 on both.

## Traps
- **Plugin names stay ASCII.** JUCE 9.0.5 double-encodes a UTF-8 `PLUGIN_NAME` ("Filtær" became "FiltÃ¦r"), and the VST3
  then failed to load in pluginval. Only the editor title says "Filtær".
- **A mode change without the duck is smooth in some cases** (stereo -> series at one cutoff): the ladder states carry
  over. Test with the filters far apart and lows plus highs in the input, or a missing duck goes unseen.
- **Resonance changes the level a lot.** Self-oscillation alone reaches -16.6 dBFS rms at cutoff 1 kHz (res 1), while the passband drops 1/(1+k).
  Numbers measured before 2026-11-20 included the old (1+k)^0.7 bass compensation (+10.9 dB at k 5); re-measure, don't copy.

## Not in v0
MS-20 and other models (next), per-stage tanh, oversampling (aliasing from the tanh is not measured), KB tracking, envelope,
CV inputs, top-end compression of the cutoff law, a different bass loss per filter, a listening verdict.
