# Filtær research, fact-checked

This is a fact-check of a raw Gemini research report (round 1; not published). The checked sources were opened on 2026-11-17. Section numbers follow the raw report. Round 2 asked for the gaps.

Status values:
- **verified**: seen in the source.
- **corrected**: the raw report was wrong; the right value is given.
- **unverified**: no source reached.
- **removed**: fabricated or unsupported.

## Sources opened

| Key | Document | URL |
|---|---|---|
| MM | Moog Matriarch manual, Rev 01/2023, p. 18 (Mixer), pp. 20-24 (filters), p. 34 (delay inputs) | https://cdn.inmusicbrands.com/Moog/Matriarch/Matriarch_Manual_012023.pdf |
| MS | Moog support, "Moog Matriarch \| Filter Modes" | https://moogmusic-help.freshdesk.com/en/support/solutions/articles/69000877830-moog-matriarch-filter-modes |
| OY | H. Oyama, "Quantifying Nonlinear Behavior in Digital Moog Ladder Filters: Cross-Implementation Comparison and Common-Core Ablation", DAFx26, Cambridge MA, Sept 2026 | https://dafx26.mit.edu/assets/papers/DAFx26_paper_29.pdf |
| DV1 | D'Angelo & Välimäki, "Generalized Moog Ladder Filter: Part I", IEEE/ACM TASLP 22(12):1825-1832, 2014 (author page) | https://dangelo.audio/taslp-ladder-part1 |
| ML | ddiakopoulos/MoogLadders, README licence table and `src/` | https://github.com/ddiakopoulos/MoogLadders |
| JL | JUCE 9.0.5 `juce_LadderFilter.cpp` and `juce_Oversampling.cpp` (a local checkout) | https://github.com/juce-framework/JUCE |
| FV | FabFilter Volcano 3 manual, "Filter panning" and routing | https://www.fabfilter.com/downloads/pdf/help/ffvolcano3-manual.pdf |

The other cited URLs answer (HTTP 200) but were not read. The one exception is the u-he Filterscape PDF on bhphotovideo.com, which returns 403. The Dubspot page cited for Cytomic The Drop is an affiliate comparison with scores, not documentation.

## 1. The Matriarch filter section

| Fact | Status | Source |
|---|---|---|
| Two filters, VCF 1 and VCF 2. Both are 24 dB/oct ladders based on the Moog 904A. | verified | MM p. 20, MS |
| VCF 2 is always low-pass. VCF 1 is high-pass or low-pass, depending on the mode. | verified | MM p. 20 |
| FILTER MODE has three positions: **HP / LP SERIES**, **LP / LP STEREO**, **HP / LP PARALLEL**. | verified | MM p. 21 |
| SERIES: Mixer → VCF 1 (HP) → VCF 2 (LP). The mono output of VCF 2 feeds VCA 1 and VCA 2. This is the "foundation for a band-pass". | verified | MM p. 21 |
| STEREO: both filters are low-pass and receive the same Mixer signal. VCF 1 goes to VCA 1 and VCF 2 to VCA 2: a true-stereo path, not summed. | verified | MM p. 21 |
| PARALLEL: VCF 1 (HP) and VCF 2 (LP) receive the same signal. Their outputs are summed to mono and feed both VCAs. This is the "foundation for a notch". | verified | MM p. 21 |
| CUTOFF sets both filters. SPACING offsets **only VCF 1**, above or below VCF 2; at centre the two are equal. | verified | MM p. 21 |
| SPACING range in octaves | not in the manual (raw report: "not found", correct). **Measured** on one unit: +5.6 to +5.9 / −5.6 oct, clockwise raises VCF 1, linear in octaves at about 1.2 oct per 30° (2026-11-17 and 2026-11-19) | MM; `matriarch-spacing-measurement.md` |
| PARALLEL sums HP and LP in phase, with equal weight | **measured** 2026-11-19 (MAIN = 1.621·VCF 1 OUT + 1.630·VCF 2 OUT) | `matriarch-spacing-measurement.md` |
| VCF 1 in HP mode, at VCF 1 OUT | **measured** 2026-11-19 at RES 0: corner follows CUTOFF with a +3-4 dB bump, but only 3-6 dB/oct below it, down to a flat leak floor; a fixed ~160-165 Hz notch at one CUTOFF value. Not the 24 dB/oct the first row of this table implies; cause open | `matriarch-spacing-measurement.md` |
| SPACING panel marks "-5 to +5", and the "secondary estimates of ±3 to ±5 octaves" | removed (no source given) | |
| Cutoff panel scale 20 Hz - 20 kHz, with marks at 200 Hz and 2 kHz | verified | MM p. 21 |
| RESONANCE 1 and RESONANCE 2 are separate knobs. The filter self-oscillates from **about 3 o'clock**. | verified (the 3 o'clock point was missing from the raw report) | MM p. 22 |
| KB TRACKING: from **no effect** (minimum) to 1 V/oct (maximum). | corrected (raw: "bipolar") | MM p. 22 |
| ENV AMT is bipolar. | verified | MM p. 22 |
| CUTOFF 1 IN is normalled to CUTOFF 2 IN. A cable in CUTOFF 2 IN affects VCF 2 only. | verified | MM p. 24 |
| CUTOFF inputs' scale | **measured** 2026-11-19: about 1.17-1.19 oct/V below about 3 kHz (calibrated against an oscillator), compressing above 4 kHz; VCF 1 about 1.2 % shallower than VCF 2 | `matriarch-spacing-measurement.md` |
| VCF 1 IN replaces the Mixer signal. VCF 2 IN replaces the Mixer or VCF 1 signal, depending on the mode. Audio inputs take -5 V to +5 V. | verified | MM p. 23 |
| There is no filter drive knob. Mixer levels above 11 o'clock add gentle distortion, and higher levels overdrive. | verified | MM p. 18 |
| Delay 1 and Delay 2 are fed from VCA 1 and VCA 2, so the filters sit before the delay. | verified | MM p. 34 |
| Bass loss at high resonance; no compensation circuit | **heard** by the listener on the Matriarch (2026-11-17: "looses heck of a lot of bass when ressonance is high"); **measured** 2026-11-19 per filter (STEREO, main outputs, 65 Hz triangle): about **−11 dB at RES noon and −15 dB at 3 o'clock**, the same on both filters; with one RES knob at a time (mixer at maximum, 32.6 Hz) each output loses 13.7-13.8 dB at 3 o'clock and the other output 0 dB; about 4 dB less with the corner at the top and at most 1.6 dB less with the mixer driven hard. Close to a linear ladder's −14 dB at k = 4 (*inference*: k not measured). Take 0017's 3.3 / 6.2 dB disagrees and is unexplained (open, unexplained). See `matriarch-spacing-measurement.md`. The manual says nothing about it | |
| Resonance still works with VCF 1 in HP mode | unverified (the manual does not say) | |
| No third-party measurements of the Matriarch filters exist | unverified | |

## 2. Software ladder models

- **Oyama (OY) is real**, and its conclusions are useful to us:
  - Linear agreement with the analog circuit does not predict nonlinear agreement.
  - Keeping the **earlier** ladder stages nonlinear preserves the SPICE harmonics better than keeping only the later ones.
  - Distributed nonlinearity matters as much as which saturator law is used.

  The raw report gets three things wrong about this paper:
  - It says "stages 1 and 4": **corrected**, the paper says earlier stages (see its table of partial linearisations).
  - It pins "boundary saturation" on MoogLadders' OberheimVariation: **corrected**. The paper's example is **JUCE's `dsp::LadderFilter`** (JUCE 8.0.13), which applies tanh at the input and in the feedback path only.
  - The FLOP counts per model are **unverified** (no source).
- **D'Angelo & Välimäki 2014**, Parts I and II: **verified** (DV1). We have not read the papers themselves yet.
- **Stilson & Smith 1996, Huovilainen 2004, Zavalishin**: the citations are plausible but **unverified**. The raw report lists them only "via" other sources.
- **JUCE `dsp::LadderFilter`** (JL):
  - It has a unit-delay feedback path, with a saturation lookup table on the input and on the feedback state s[4].
  - Its constants 0.769 / 0.231 are the Huovilainen-style stage. **Verified.**
  - The resonance scaling "k / 4.5" was **not found** in the source.
- **The JUCE licence is the JUCE 9 licence or AGPLv3**: **corrected** (raw: GPLv3).
- **MoogLadders licences** (ML), **verified** against the README:

  | Model | Licence | Closed source OK |
  |---|---|---|
  | Simplified | custom (DAFX) | no |
  | Huovilainen | LGPLv3 | only if dynamically linked |
  | Stilson, Microtracker, Krajeski, Hyperion | Unlicense | yes |
  | MusicDSP | suggested CC-BY-SA | yes |
  | Oberheim | custom (Will Pirkle) | yes |
  | Improved (D'Angelo) | ISC | yes |
  | RKSimulation | BSD | yes |

  The raw report left out MusicDSP. Note that CC-BY-SA is share-alike.
- **sst-filters (Surge) is GPLv3**: the LICENSE page exists, but we did not read the text. Reference only.
- **Oversampling: corrected.** The raw report says JUCE's polyphase IIR oversampling adds zero latency, with negligible phase below 15 kHz. JUCE computes a latency for that path and reports it (`juce_Oversampling.cpp:285-289`). The latency is small but not zero. Whether "small" meets our zero-latency rule is still open. ADAA at 1x, which round 2 asks about, would avoid the question.

## 3. Other filter characters

The descriptions of character are opinion and are kept as such. Citation problems:
- **Parker & D'Angelo, DAFx-13**, is the Buchla lowpass-gate paper. **Corrected:** it is not a Korg35 or diode-ladder model.
- **Wishnick, DAFx-14**, "Time-Varying Filters for Musical Applications", is cited for Steiner-Parker: **unverified**. The title is general.
- **Zavalishin chapter numbers** (Ch. 5 Sallen-Key, Ch. 6 diode ladder): **unverified**.
- **The CPU multipliers** (0.4x to 1.6x): **removed**, as they have no source.
- **Open code:** `juce::dsp::StateVariableTPTFilter` falls under the JUCE licence. The Pirkle code is under the book's licence (unverified). GPL code (sst-filters, Odin 2, VCV modules) is reference only.

The recommendation is marked as inference in the raw report: the SEM 12 dB state-variable filter and the MS-20 as the two contrasts to the ladder. It is reasonable, and the author has independently named the MS-20 (section 7).

## 4. Comparable products

- **FabFilter Volcano 3** (FV), **verified**:
  - A per-filter pan ring filters the left channel lower and the right higher (or the other way round). This is the closest prior art to SPACING.
  - **Corrected:** the raw report says the pan ring is "identical to SPACING in stereo/M-S". Panning is **disabled** in the L/R and M/S routing modes.
  - The offset is symmetric (L and R move in opposite directions). SPACING moves VCF 1 against a fixed VCF 2.
  - Pan is a modulation target (FV, modulation section).
- **Not answered** in the raw report: Xfer, and "plugins that copy the Matriarch modes". Both go into round 2.
- **Unverified:**
  - Arturia Filter M12 and Filter MINI, Soundtoys FilterFreak (manual answers, not read), MF-101 / MF-101S, Mother-32.
  - Cytomic The Drop's "Spread" knob: the only source is the affiliate page.

## 5. Placement in an echo

- The Matriarch puts the filters before the delay: **verified** (MM p. 34).
- The hardware and plugin examples of a filter inside the feedback loop have no sources: **unverified**.
- **Corrected:** the raw report says a ladder loses "up to 12 dB" of passband gain. A linear ladder's DC gain is 1/(1+k), which is about **-14 dB** at k = 4.
- The loop-gain argument is correct as linear algebra: with feedback 0.5 and a +12 dB peak, the loop gain at the peak is +6 dB, so the peak runs away.
- The mitigations are generic but sound:
  - a DC blocker after asymmetric saturation;
  - a soft clipper in the delay loop;
  - optional passband compensation.

  TidBank's feedback loop already has a limiter and a breaker.

## 6. The C++ prototype (second turn of the share)

**It does not work. Do not use it.** We ported `LadderCore` to Python unchanged, with its own Padé `fastTanh`, a 0.1-amplitude sine, cutoff at 1 kHz and fs = 48 kHz.

| k | 100 Hz | 1 kHz | 2 kHz | 8 kHz |
|---|---|---|---|---|
| 0 | -3.1 dB | -3.1 dB | -3.1 dB | -3.7 dB |
| 2 | state → inf | inf | inf | inf |
| 3.5 | inf | inf | inf | inf |

An analog 4-pole ladder at k = 0 gives -0.2 / -12.0 / -28.0 dB at 100 Hz / 1 kHz / 2 kHz. The prototype does not low-pass.

Likely cause (inference, not tested on its own): each stage computes `G*(x - s) + s` with `s = (1-G)*z`. That applies (1-G) to the state a second time, so the stages do not match the Σ the code uses to solve the feedback. The state update `y + (y - s)*(1/G - 1)` does not fit the TPT form either.

## 7. Design direction (the author, 2026-11-17)

In short: summing the two filters to one output is only ONE of the Matriarch's options. It also has a parallel LP path where the filters stay separate and act as stereo. The plugin should offer selectable configurations, not one fixed routing.

What follows from it, as a proposal and not yet decided:
- **A configuration is a routing plus a filter model per slot**, chosen from one list.
  - Matriarch-style routings:
    - SERIES HP→LP, summed to mono.
    - PARALLEL HP+LP, summed to mono.
    - STEREO LP/LP: VCF 1 → L, VCF 2 → R, not summed.
  - Stereo extensions:
    - SERIES or PARALLEL as a pair per channel (four filter cores).
    - Model swaps, for example "MS-20 stereo": an MS-20 HP→LP chain per channel.
- **The input in STEREO needs a choice.** The hardware feeds both filters the same Mixer signal. A plugin with stereo input can do the same (from a mono sum), or send L → VCF 1 and R → VCF 2, as patching VCF 1 IN and VCF 2 IN does.
- **SPACING keeps its Matriarch meaning in every configuration:** it offsets VCF 1 (or the first filter of each pair) against the second.
- **Decided for v0 (d38, 2026-11-18; options 1 and 2 of this list, the rest later):** its own plugin, the ladder only, the Matriarch's three
  routings, both filters fed the mono sum. The MS-20 is next. Built in `dsp/Filtaer.h` (skill `filtaer-plugin`).
- **Still open:** one flat menu of named configurations, or two selectors (routing × model)? Moot until a second model.

## 8. Open questions

**To measure on a real Matriarch** (from the raw report, kept; they need a Matriarch and an audio interface):
1. **SPACING range and law.** *Measured (2026-11-17, 2026-11-19): +5.6 to +5.9 / −5.6 oct, about 1.2 oct per 30°, linear in octaves. Open: whether the filters' tracking drift depends on SPACING.* In STEREO, with both filters self-oscillating and CUTOFF at about 1 kHz, measure the frequency at VCF 1 OUT as SPACING sweeps.
2. **VCF 1 in HP mode.** *Partly measured 2026-11-19 (RES 0 only: shallow skirt, leak floor, a fixed notch); resonance curve and the leak's source open.* Measure the slope and the resonance curve at VCF 1 OUT, with RESONANCE 1 at 0 %, 50 %, 90 % and 100 %.
3. **Low-pass bass loss against resonance.** The listener has heard it; this measures how much. *Measured 2026-11-19: about 11 dB at noon, 15 dB at 3 o'clock, per filter. Take 0017's smaller figures are unexplained (open, unexplained).*
   - Setup: LP/LP STEREO, OSC 1 triangle on a low note (about 55 Hz), and only OSC 1 up in the mixer, below 11 o'clock (MM p. 18: drive starts above that). CUTOFF at the 2 kHz mark (about 2.7 kHz on the measured unit), DRONE, ENV AMT at centre, KB TRACKING at minimum.
   - Take: step RESONANCE 0, 25 %, 50 %, 75 % and 100 %, holding each for 3 s.
   - Read: the level of the 55 Hz fundamental per step. A linear ladder predicts 1/(1+k), about −14 dB at k = 4.
4. **Mixer drive.** Feed a 100 Hz sine into the instrument input, sweep the Mixer level, and read the 2nd-to-3rd harmonic ratio at VCF 2 OUT with the cutoff fully open.
5. **PARALLEL phase.** *Answered 2026-11-19: in phase, equal weight.* With SPACING at 0 and the resonances at 0, measure whether HP and LP sum in phase or out of phase at the crossover.

**Gaps that documents may still close:** round 2 asked for them; the answers are in section 9.

## 9. Round 2 fact-check

This checks a second raw Gemini report (not published). The round ran in "Code & analysis" mode again, not Deep Research. It is better grounded than round 1: almost every cited URL answers. We opened the sources behind the claims that shape the design.

**Sources opened in round 2:**

| Key | Document | URL |
|---|---|---|
| TS | T. E. Stinchcombe, "A Study of the Korg MS10 & MS20 Filters", 2006 | https://www.timstinchcombe.co.uk/synth/MS20_study.pdf |
| KM | Korg, MS-20 mini developer interview | https://www.korg.com/uk/products/synthesizers/ms_20mini/page_4.php |
| KF | Korg, MS-20 FS product page | https://www.korg.com/meen/products/synthesizers/ms_20fs/ |
| VZ | Zavalishin, "The Art of VA Filter Design", rev. 2.1.2, table of contents | https://www.discodsp.net/VAFilterDesign_2.1.2.pdf |
| CD | Cytomic, The Drop manual | https://cytomic.com/files/TheDrop-Manual.pdf |
| AM | Arturia, Filter M12 manual 1.2.0 | https://dl.arturia.net/products/m12-filter/manual/filter-m12_Manual_1_2_0_EN.pdf |
| BW | Bitwig user guide, Filter+ and Sweep | https://www.bitwig.com/userguide/latest/filter15/ |
| CC | u-he, Colour Copy user guide | https://uhe-dl.b-cdn.net/manuals/plugins/colourcopy/ColourCopy-user-guide.pdf |
| EB | Soundtoys, EchoBoy manual | https://www.soundtoys.com/wp-content/uploads/EchoBoy-Manual.pdf |
| MF | Moog, MF-104S manual | https://moogconnect.net/m/mfs/mf-104s.html |
| SP | Giampiccolo et al., DAFx26 paper 25 (KAN vs MLP in WDFs, MiniBrute VCF) | https://dafx26.mit.edu/assets/papers/DAFx26_paper_25.pdf |
| MG | midi.guide, Matriarch CC list (secondary) | https://midi.guide/d/moog/matriarch/ |
| GH | GitHub API: licence and file of each cited repository | |

**Claims that matter for the design:**

| Claim (round 2) | Status | Source |
|---|---|---|
| SPACING range, VCF 1 HP switching, measured bass loss: not published | consistent with round 1 (no new source) | |
| The Matriarch's filter knobs have no MIDI CC; Delay Spacing is CC 13 and the noise filter cutoff is CC 9 | consistent with MG (secondary). The 14-bit LSB claim (CC 45) is verified over MIDI 2026-11-19: CC45 at CC13 64 moves Delay 1 134.6 -> 138.4 ms over LSB 0-127, about one MSB step (`matriarch-measurement.md`, Results 2026-11-19) | MG, measured |
| RESONANCE 1 works when VCF 1 is in HP mode | unverified: the source is the CDM guide (secondary, Moog-sponsored) | |
| The MS-20 mini uses the early KORG35 filter | verified | KM |
| The MS-20 FS has both the early and the late filter | verified | KF |
| Early MS-20 filters use the Korg35; later ones use the LM13600 OTA | verified | TS §1 |
| The MS-20 HPF and LPF are both 12 dB/oct | **corrected:** both high-pass variants (Korg35 and OTA) are **6 dB/oct**. The low-pass is 12 dB/oct | TS §7, p. 39 |
| Korg35 diodes sit in the resonance feedback path | **corrected:** in the Korg35 filter the diodes are in the **forward path**. In the OTA version they are in the feedback loop (Gemini has the OTA version right) | TS §2 |
| Zavalishin: Ch. 4 SVF, Ch. 5 "Sallen-Key filters", Ch. 6 "Diode ladder filters" | **corrected** (rev. 2.1.2): Ch. 4 is the state-variable filter; Ch. 5 is the ladder filter, with **§5.8 Sallen-Key** and **§5.10 diode ladder** (also §5.6 HP ladder and §5.5 multimode ladder) | VZ contents |
| Parker & D'Angelo DAFx-13 is the Buchla lowpass gate; Wishnick DAFx-14 covers time-varying SVF and biquad stability, not Steiner-Parker | agrees with our round-1 correction (Wishnick PDF answers; the abstract was not read) | |
| The Steiner-Parker reference is DAFx26 paper 25 | **partly:** the paper is real, but it is a KAN-versus-MLP study of the **feedforward low-pass stage** of the MiniBrute VCF, not a full filter model | SP |
| The Drop: Serial (HP→LP) and Parallel (HP+LP) routing; models including MS2 (MS-20 rev 2, OTA); resonance self-oscillates above 100 %, and a SAFE switch keeps it below | verified | CD pp. 11-12 |
| The Drop has no static cutoff offset; its "Spread" is only an LFO spread | **corrected:** the Cutoff Section has **SHIFT** (moves L and R cutoffs together) and **SPREAD** ("moves the left and right cutoffs away from each other"). The LFO has a separate SPREAD | CD p. 12 |
| Arturia Filter M12 runs two filters in parallel ("for stereo processing") or in series, with 15 types each | verified (the exact names of the routing buttons were not checked) | AM |
| Bitwig Sweep: a Routing Blend from 0 % (A only) through 50 % (parallel) to 100 % (serial); Joint Frequency Control ±3 octaves; Invert moves the two cutoffs in opposite directions | verified | BW |
| Colour Copy: highpass, lowpass, compressor and expander are in the feedback loop; RESO colour "prevents resonance build-up" with a slow compander; there is a PANIC control | verified | CC |
| EchoBoy: feedback near maximum drives it into "runaway"; saturation styles include Hard Limit | verified. Low and high cut in the loop: unverified | EB |
| MF-104S: feedback above 8 can self-oscillate | verified. The quoted LEGACY wording "feedback path through the whole circuit" was not found; the manual says LEGACY reproduces the original's dark tone | MF |
| Valhalla Delay "filter in the feedback path", RE-201 block diagram, Strymon quotes | unverified (403 or not read) | |

**Open-source code: licence and file, checked through the GitHub API:**

| Character | Repository and file | Licence | Status |
|---|---|---|---|
| MS-20 (Korg35 and OTA) | `filliformes/aphex-move`: `src/dsp/k35.h`, `k35_ota.h` | MIT | verified. A hobby project from 2026 with one star; it calls itself "clean-room". Quality unknown |
| MS-20, SEM (Oberheim), diode ladder, Sallen-Key | `grame-cncm/faustlibraries`: `vaeffects.lib` (`korg35LPF/HPF`, `oberheim*`, `diodeLadder`, `sallenKey*`) | each function declares `LicenseRef-STK-4.3` (MIT-style) | verified. Faust compiles to C++ |
| Diode ladder | `PaulBatchelor/Soundpipe`: `modules/diode.c` | MIT (repository) | verified. The file says it is "based on Will Pirkle's Diode Ladder filter design", so the licence of the original design is unverified |
| OTA 4-pole (CEM3320) | `mamedev/mame`: `src/devices/sound/cem3320.cpp` | file header `license:BSD-3-Clause` | verified (the MAME repository as a whole is mixed) |
| WDF building blocks | `Chowdhury-DSP/chowdsp_wdf` | BSD-3-Clause | verified |
| OTA cascade "ripples/dsp/filter.h" | `pichenettes/eurorack` | | **removed:** `ripples/` holds only `hardware_design`. `stmlib/dsp/filter.h` (an SVF, by Emilie Gillet) exists; STM32 code is MIT under the README |
| GPL reference only | sst-filters (Surge), chowdsp_utils filters | GPL-3.0 (as stated by the report) | not opened |

**Anti-aliasing:**
- **The cited ADAA papers answer** (Holters DAFx-19, Albertini et al. DAFx-20, La Pastina & D'Angelo DAFx20in22). Only the Holters PDF was downloaded, and we did not check his equations.
- The report says no published stateful ADAA has been applied to a full 4-pole ladder or a Sallen-Key filter. That is plausible but **unverified**.
- **JUCE latency at 2x with IIR:**
  - The report says the value was "not found".
  - In fact JUCE computes it from the phase response of the half-band filters at a low frequency (`juce_Oversampling.cpp:285-289`). It is not a literal, so the number is still to be read with a JUCE call in a test.

**What changes for the design:**
- **An MS-20 model offers two real variants:** the early Korg35 (diodes in the forward path) and the late OTA (diodes in the feedback loop). The MS-20 FS ships both, so both are legitimate list entries.
- **Its HPF is 6 dB/oct, not 12.** A faithful MS-20 configuration therefore has an asymmetric band (6 dB below, 12 dB above).
- **Prior art for a stereo cutoff offset:**
  - The Drop's SPREAD (static, symmetric L/R);
  - Volcano's pan ring (symmetric, disabled in L/R and M/S modes);
  - Bitwig Sweep's Invert (moves A and B apart, ±3 oct).

  None of them moves one filter against a fixed one, as the Matriarch's SPACING does. That keeps SPACING a distinguishing control.
- **Self-oscillation is an expected feature.** The Drop's SAFE switch and Colour Copy's RESO compander and PANIC are the patterns for living with it inside a feedback loop.

## Runs

| Run | Model | Scope | File |
|---|---|---|---|
| 1 | Gemini, "Code & analysis" mode, 2026-11-17 | the whole round-1 prompt, plus a C++ prototype on request | raw report (not published) |
| 1c | Claude Opus 5.5 | fact-check of run 1 against MM, MS, OY, DV1, ML, JL and FV, plus a simulation of the prototype | this file |
| 2 | Gemini, "Code & analysis" mode, follow-up, 2026-11-17 | the round-2 prompt, plus an ADAA code turn (not checked) | raw report (not published) |
| 2c | Claude Opus 5.5 | fact-check of run 2: the sources in section 9 opened, repository licences read through the GitHub API | this file, section 9 |

