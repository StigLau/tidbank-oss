# Measuring the Matriarch's filter SPACING (patch cables and Live only)

**Why:** Moog publishes neither the SPACING range nor its law (`filter-matriarch.md`, sections 1, 8 and 9). Filtær's SPACING should match the hardware.

**How:** in LP/LP STEREO mode at full resonance, each filter rings as a sine on its own output channel. VCF 2 stays put, and VCF 1 moves with SPACING. Record the stereo output while stepping SPACING through its marks; `tools/matriarch/spacing.py` turns the recording into octaves per mark. No probing or opening the unit is needed.

The panel facts below are from the Matriarch manual (Rev 01/2023):
- p. 21: filter modes.
- p. 22: RESONANCE self-oscillates from about 3 o'clock; ENV AMT is bipolar, with no effect at centre; KB TRACKING has no effect at minimum.
- p. 29: VCA MODE DRONE holds the VCAs open without a key.

## You need

- The Matriarch's main L and R outputs into two inputs of your audio interface.
- Live recording one stereo track at 48 kHz (any rate works).
- No cables in the filter jacks (VCF IN/OUT, CUTOFF 1/2 IN, ENV AMT IN). Nothing else patched to the filters either.

## Panel setup

| Control | Setting | Why |
|---|---|---|
| FILTER MODE | LP / LP STEREO | VCF 1 → VCA 1, VCF 2 → VCA 2: one filter per channel |
| Mixer: every OSC and NOISE level | minimum | the filters should ring on their own, with no input |
| RESONANCE 1 and 2 | fully clockwise | self-oscillation, which gives a clean sine per filter |
| ENV AMT | exactly centre (12 o'clock) | otherwise the filter envelope moves both pitches |
| KB TRACKING | fully counterclockwise | keys do not move the cutoff |
| Mod wheel, pitch bend | at rest | no modulation |
| VCA MODE | DRONE | the VCAs stay open without holding a key |
| Delay MIX | fully dry | no echoes of the previous step |
| MAIN VOLUME | so that Live peaks around -12 dBFS | no clipping |

Wait a minute after switching on: analog filters drift while they warm up. The script reports VCF 2's drift so you can see whether it settled.

## Takes

Move SPACING briskly between marks and **hold each mark for at least 3 seconds**. The script finds the steady stretches, one row per mark, in order. Write down the marks you used, in order.

1. **Take A, upward:** set CUTOFF to the 200 Hz mark. Start with SPACING at centre, then step through every mark clockwise to the end of travel.
2. **Take B, downward:** set CUTOFF to the 2 kHz mark. Start at centre, then step through every mark counterclockwise to the end of travel.
3. **Take C, optional (the law):** with the CUTOFF from take A, set SPACING at centre, then halfway between centre and the first mark, the first mark, and halfway to the second. If the offsets in octaves grow evenly with knob angle, the law is exponential in frequency (V/oct), like CUTOFF.

The two cutoff settings keep VCF 1 inside the audio band at the extremes. If a mark yields no row, VCF 1 left the range where it oscillates or that the interface records. That is still a result: note it as "beyond the limit at this CUTOFF".

Export each take as a WAV (or use the recorded clip's file).

## Analyse

```bash
uv run --with numpy tools/matriarch/spacing.py measure take-a.wav
uv run --with numpy tools/matriarch/spacing.py measure take-b.wav
uv run --with numpy tools/matriarch/spacing.py selftest     # checks the script itself on a synthetic take
```

The output is one row per steady stretch: VCF 1 and VCF 2 in Hz, the offset in octaves, the level, and VCF 2's drift in cents. Add `--json` for machine-readable output. Keep the output next to your list of marks.

## Notes and leads

A Google search agent (2026-11-17) suggested setting ENV AMT to centre and using DRONE. The manual confirms both, and both are in the protocol above. Its other claims are leads only, unverified:
- **A SPACING MIDI parameter (00-7F, centre 3F) in a SunnySynths MIDI reference.** This conflicts with midi.guide, which lists no filter CC.
- **ModWiggler topic 249256**, said to reverse-engineer the VCF and VCA stages with plots. Cloudflare blocks automated reading; a human can open it.
- **High-frequency rolloff on the main outputs**, and a loudness change at 8-12 kHz during sweeps. Neither affects this measurement, which compares two pitches, not levels.
- **A "SPACING dead in Stereo mode" fault, fixed by pot calibration via SysEx.** If VCF 1 does not move in take A, check that first.
- **No public service manual.** This agrees with what we found.

## Results, 2026-11-17 (one Matriarch unit, through the Tascam Model 24, recorded in Live)

**The chain:** in every take, one Matriarch output is copied to both channels (L and R are identical). The takes are kept locally, not in the repo (Live projects of 2026-11-17).

**Modes:** takes 0009 and 0010 are in LP/LP STEREO, at full resonance. Takes 0001-0005 may have been in SERIES HP/LP (per the operator, 2026-11-17).

### CUTOFF: take 0010, LP/LP STEREO

The take is 66 s long, and CUTOFF moved with SPACING at centre. Both filters move together, as two tones a few dB apart: VCF 2 is the louder and lower one. At the bottom of the knob the output goes completely silent (−91 dBFS), so no fixed filter remained.

| CUTOFF | Tone |
|---|---|
| 12 o'clock | **333 Hz** at the start, **329 Hz** at the end |
| 200 Hz mark | most likely **38.5 Hz**, held 2.5 s (14-16 s); take 0012, at the 200 Hz mark as the operator recalls it, gives 41.4 Hz. Brief stops at 172 Hz and 153 Hz on the way were not marks |
| 2 kHz mark | **2741-2745 Hz**, held 3 s |
| fully counterclockwise | the tone fades out: 30.8 Hz at −33 dB, 20.5 Hz at −46 dB and −71 dB, then silence from 38 s |
| fully clockwise | VCF 2 at **21.44 kHz**, steady (−41 dB); VCF 1 at **22.75 kHz** |

**The marks in between do not match the pitch.**
- The 200 Hz mark gives about 40 Hz, 12 o'clock gives 333 Hz, and the 2 kHz mark gives 2745 Hz.
- That puts 3.1 octaves on each side of 12 o'clock, so the panel decade from 200 Hz to 2 kHz (3.3 oct) spans 6.1 octaves of pitch.
- This is at full resonance (self-oscillation), with KB TRACKING and ENV AMT at zero.
- For Filtær: take the end points from the faceplate, and treat the printed marks as labels, not as calibration.

**The filters oscillate from about 20 Hz to at least 22.75 kHz** (VCF 1, take 0010). That roughly matches the faceplate's 20 Hz - 20 kHz, and no ceiling was found. At the bottom the tone fades out instead of stopping at a frequency. Take 0001 (45 Hz - 15.7 kHz) is in a mode that is not known, so it does not count.

**Two tones at 12 o'clock:** with SPACING at 12 o'clock the two filters do not sit in unison (see SPACING below), so two tones are present at every CUTOFF setting: 333.5 and 418.8 Hz at the start, 38.5 and 49.1 Hz at the 200 Hz mark, and 2741 and 3310 Hz at the 2 kHz mark. The CUTOFF figures above are VCF 2's.

### SPACING: takes 0012, 0009 and 0011 (LP/LP STEREO), and take 0005 for the counterclockwise end

**Method note:** frequencies at the steady stretches come from long (1.3-6 s) Blackman-windowed FFTs. An earlier pass used a 0.25 s tracker that took the two strongest peaks at least half an octave apart. That tracker missed the second filter whenever the two were closer than half an octave, and produced a false "×1.41 second tone". Both errors are fixed below.

**At 12 o'clock VCF 1 sits above VCF 2.** The gap is about a third of an octave at low cutoff and shrinks higher up. It also depends on where 12 o'clock was set by eye: the same take, 0009, gives +0.41 at its start and +0.10 at its end.

| Take | VCF 2 | VCF 1, SPACING at 12 o'clock | Offset |
|---|---|---|---|
| 0010 (CUTOFF at the 200 Hz mark) | 38.5 Hz | 49.1 Hz | +0.35 oct |
| 0012, start / end | 41.2 / 41.3 Hz | 51.4 / 53.9 Hz | +0.32 / +0.38 oct |
| 0010 (CUTOFF at 12 o'clock) | 333.5 Hz | 418.8 Hz | +0.33 oct |
| 0009, start / end | 440.1 / 441.2 Hz | 584.6 / 473.2 Hz | +0.41 / +0.10 oct |
| 0010 (CUTOFF at the 2 kHz mark) | 2741 Hz | 3310 Hz | +0.27 oct |
| 0010 (CUTOFF fully clockwise) | 21.44 kHz | 22.75 kHz | +0.09 oct |

Take 0017 seemed to find the filters in unison at 12 o'clock (2433 Hz), but that take is unconfirmed. On 2026-11-19, "12 o'clock" set by eye gave between about 0 and +0.45 oct (track-t1, track-cv-t1), so the knob's centre is near unison but a by-eye setting is not; the offsets here are most likely that. For Filtær: SPACING zero means unison (*inference*).

**Take 0012 measures the clockwise end** (34 s; SPACING from 12 o'clock to fully clockwise and back; CUTOFF at the 200 Hz mark as the operator recalls). VCF 2 stays at 41.3 Hz, and VCF 1 at the clockwise end is **2478 Hz** (held 6 s).

| | Clockwise end | Counterclockwise end |
|---|---|---|
| against VCF 2 (true unison) | **+5.91 oct** (take 0012) | **−5.63 oct** (take 0005: 57.9 Hz against 2860 Hz, held 3.5 s) |
| against VCF 1 at 12 o'clock | +5.52 to +5.59 oct | −5.84 oct (using take 0005's starting tone, 3322 Hz) |

**SPACING spans about 11.5 octaves end to end: about +5.9 and −5.6 octaves around unison.** SPACING moves VCF 1 only; VCF 2 does not move in any take.

The other takes agree:
- **0011:** VCF 1 is steady at 872 Hz fully clockwise. Then +5.9 oct puts VCF 2 at about 15 Hz, below the oscillation floor, which fits the silence at 12 o'clock.
- **0009:** fully clockwise, VCF 1 sits at 20.66 kHz, **+5.55 oct** against VCF 2 at 440 Hz, less than take 0012's +5.91 from 41 Hz. That is no ceiling (VCF 1 reaches 22.75 kHz in take 0010). It fits the compression at the top of the range that the CV takes show (below).

Take 0005 may have been in SERIES. *Inference:* SPACING offsets VCF 1's cutoff the same way in any mode.

**The knob law: take 0019.** LP/LP, CUTOFF unchanged throughout, RESONANCE 1 fully clockwise and RESONANCE 2 at 0, so only VCF 1 rings. SPACING was set by eye to clock positions. Afterwards RESONANCE 1 went to 0 and RESONANCE 2 up: VCF 2 rang at **314.2 Hz**, and moving SPACING did not change it. That is the unison reference.

| SPACING | VCF 1 | Against VCF 2 (314.2 Hz) | Step from the previous mark |
|---|---|---|---|
| 12 o'clock (first) | 397.4 Hz | +0.34 oct | |
| 1 | 1072 Hz | +1.77 oct | +1.43 |
| 2 | 2284 Hz | +2.86 oct | +1.09 |
| 3 | 5305 Hz | +4.08 oct | +1.22 |
| 4 | 11.44 kHz | +5.19 oct | +1.11 |
| 5 | 15.31 kHz | +5.61 oct | +0.42 (top-end compression) |
| 12 o'clock (second) | 321.9 Hz | +0.04 oct | |
| 11 | 117.6 Hz | −1.42 oct | −1.45 |
| 10 | 53.5 Hz | −2.55 oct | −1.14 |
| 9 and below | silent: below the oscillation floor (about 20 Hz, −4 oct) | | |
| 12 o'clock (third) | 436.3 Hz | +0.47 oct | |

- **SPACING is exponential in knob angle:** away from the ends, each hour mark (30°) adds about **1.1-1.2 octaves** (1.09, 1.22, 1.11 and 1.14). The first hour from 12 o'clock reads 1.43-1.45 in both directions. That may be a steeper law near centre, or the by-eye setting of 12 o'clock; this take cannot tell.
- **The total checks out:** about 1.15 oct per 30° over the knob's 300° gives about 11.5 octaves, the span measured from the ends (+5.91 / −5.63 oct).
- **12 o'clock set by eye** gave +0.04, +0.34 and +0.47 oct in one take. Near centre, SPACING moves about 0.04 oct per degree, so a few degrees are a third of an octave.
- **Both filters on one output:** with RESONANCE 1 at 0, VCF 2 rang on the same output at −23 dB, louder than VCF 1 had been (−27 dB). The output was not VCF 1 alone; which mode or output carried both is not recorded. This does not affect the pitches.
- **For Filtær:** SPACING in octaves, linear in knob position, +5.6 to +5.9 / −5.6 oct; optionally slightly steeper near centre.

### CUTOFF 1 IN under CV: takes 0014 and "0012 [181259]" (LP/LP STEREO)

The CV source is a triangle (per the operator). Its voltage is not measured here.
- **Take 0014, part 1 (0.5-10.7 s):** SPACING at 12 o'clock, a dead patch in CUTOFF 2 IN. VCF 2 stays at **476.1 Hz**. VCF 1 sweeps up and down, from below the oscillation floor to the top of the band, with a period of about 5.4 s.
- **Part 2 (12-21.6 s):** no dead patch. The CV is normalled to both filters, and they sweep together.
- **"0012 [181259]":** the same takes with SPACING fully clockwise (by mistake). VCF 2 stays at 474.8 Hz, and VCF 1 stays mostly above 700 Hz.

Every clean ramp in part 1, tracked with 50 ms frames from 150 Hz to 12 kHz:

| Ramp | From → to | Slope | Deviation from a straight line in octaves (rms) |
|---|---|---|---|
| 1.40-2.04 s | 158 → 1209 Hz | +4.60 oct/s | 25 cents |
| 4.04-4.82 s | 1895 → 153 Hz | −4.65 oct/s | 26 cents |
| 6.84-7.64 s | 167 → 2111 Hz | +4.59 oct/s | 15 cents |
| 9.62-10.24 s | 1177 → 154 Hz | −4.65 oct/s | 20 cents |

**Take 0015, the reverse:** the same triangle into CUTOFF 2 IN, with a dead patch in CUTOFF 1 IN (removed after a pause at about 11 s). VCF 1 stays at **488.2 Hz** throughout, both before and after the dead patch is removed. This confirms the manual (p. 24): CUTOFF 2 IN reaches VCF 2 only, and only CUTOFF 1 IN is normalled across. VCF 2's ramps:

| Ramp | From → to | Slope | rms deviation from a straight line |
|---|---|---|---|
| 0.59-1.34 s | 1775 → 152 Hz | −4.74 oct/s | 14 c |
| 3.37-4.54 s | 152 → 6563 Hz | +4.64 oct/s | 20 c |
| 5.63-6.76 s | 6037 → 154 Hz | −4.71 oct/s | 17 c |
| 8.79-9.95 s | 154 → 6092 Hz | +4.64 oct/s | 18 c |
| 11.69-23.05 s (part 2, six ramps) | 151 Hz ↔ 6.4 kHz | +4.64 / −4.71 to −4.74 oct/s | 13-19 c |

**The CV was a triangle (per the operator), so both CUTOFF inputs are exponential up to about 4 kHz, and compress above that.**
- From 150 Hz to 4 kHz the ramps are straight in octaves within a few cents: 1-7 cents rms in the blind check, and 13-26 cents with 50 ms frames above.
- Above 4 kHz the rate drops on both filters. VCF 2 (take 0015) runs at 4.59-4.70 oct/s at 1-4 kHz and at 4.41-4.54 oct/s at 4-8 kHz. The blind check found 3.9-4.1 oct/s at 8-14 kHz.
- In Filtær: exponential cutoff, with optional compression at the top.
- **Period:** 5.42-5.43 s, from the times successive sweeps cross 250 Hz and 3 kHz.
- **Peak to peak at the filter**, extrapolated from the slopes below 4 kHz: the period divided by (1/up slope + 1/down slope). That gives **12.7 octaves for VCF 2** and **12.6 for VCF 1** (take 0014). The real sweep is narrower at the top because of the compression.
- **The two inputs match within about 1-2%** (VCF 2 slightly steeper): 1% here, 1.7% in the blind check.
- **Volts per octave: about 1.25-1.28 oct/V between 150 Hz and 4 kHz** *(probably too high: calibrated against an oscillator on 2026-11-19 it is about 1.18 oct/V; see "Calibrating the CUTOFF inputs in volts" below)* (slope × period / 20 V; VCF 2 slightly above VCF 1), and less above 4 kHz. The triangle came from the Utilities LFO's TRI OUT, unattenuated (per the operator), and that output is specified as 10 V peak to peak (manual p. 44). The voltage was not measured, so the figure rests on that spec. Both CUTOFF inputs are therefore steeper than 1 V/oct.

**Take 0016, a sine at several speeds** (111 s): the Main LFO's sine into CUTOFF 1 IN, normalled to both filters (per the operator). Its rate was swept by hand from very slow to audio rate. The output is specified as 10 V peak to peak (manual p. 39).
- **The slow stretch** (22-48 s; period **16.06 s**) fits a sine in octaves at **214 cents rms** over 150 Hz to 14 kHz. A sine in Hz (a linear law) fits several times worse: 870-3786 cents, depending on how the fit is set up. So the law is exponential again. The amplitude is not reliable: the full-band fit gives 12.2-12.5 octaves (1.22-1.25 oct/V), and the blind check, fitting only below 4 kHz, gives 14.1-14.4 octaves (about 1.4 oct/V). The top-end compression bends the fit, so **use the triangle's figure, not this one, for volts per octave.** The bottom of each cycle is silent for about 5 s.
- **The fit is worse than the triangle's** (214 against 15-26 cents). The likely cause is the top-end compression the triangle takes show.
- **The faster stretches** are about 0.4-0.8 Hz (50-58 s), about 3-15 Hz (0-11, 59-72 and 92-106 s), about 40 Hz (70-74 s), and an audio-rate sine at **56-62 Hz** (72-92 s), where the output becomes a steady complex tone (filter FM). These rates come from the output's level envelope, so they are approximate.
- How far the sweep reaches at the faster rates was not measured. The output's level swing shrinks from about 70 dB (slow, silent at the bottom) to about 30 dB (3-15 Hz) and about 18 dB (audio rate), but that mixes sweep depth with how fast self-oscillation can die and restart.

### Bass loss against resonance: take 0017 (LP/LP STEREO, SPACING at 12 o'clock)

> **Contradicted 2026-11-19, unexplained:** per-filter takes give about −11 dB at RES noon and −14 to −15 dB at 3 o'clock on each output (bass-st-t3, normal mixer level); with one RES knob at a time and the mixer at maximum, −13.7 / −13.8 dB on the main outputs (split-res1/2). Do not use the figures below until the difference is explained. See "Bass loss against resonance, repeated with take 0017's setup" and "One knob at a time" below.

The take is 44 s long. The source is OSC 1, a triangle at **65.2 Hz** (C2). The recording peaks at −2.6 dBFS, so it does not clip. RESONANCE was stepped up from zero and back, recorded first from the **left** output (0-25 s, VCF 1; seven stops) and then from the **right** output (26-44 s, VCF 2; five stops). Each value is the median of a 2-3 s stop (Blackman FFT).

| Stop | Left (VCF 1): 65 Hz fundamental | Right (VCF 2): 65 Hz fundamental |
|---|---|---|
| resonance 0 | −2.93 dBFS (reference) | −2.93 dBFS (reference) |
| 1 | −1.34 dB | −2.34 dB |
| 2 | −2.29 dB | −4.72 dB |
| 3 | −2.66 dB | −5.82 dB; self-oscillation starts at 2432 Hz |
| 4 | −3.01 dB | **−6.19 dB**; self-oscillation at 2405 Hz |
| 5 | −3.22 dB; self-oscillation starts at 2433 Hz | |
| 6 | **−3.34 dB**; self-oscillation at 2405 Hz | |

- **The bass drops by 3.3 dB on the left and 6.2 dB on the right at full resonance.** That is far less than a linear ladder's 1/(1+k) (about −14 dB at k = 4), so the Matriarch does not behave like the textbook linear ladder here.
- **The two outputs behave differently.** On the right, the 3rd and 5th harmonics (196 Hz and 326 Hz) fall about 8 dB at the first stop, against about 2 dB for the fundamental. On the left all harmonics fall roughly together. Cause unknown.
- **RESONANCE 1 was turned for the left part and RESONANCE 2 for the right part** (per the operator). So the difference is between the two filters: **VCF 1 loses 3.3 dB, VCF 2 loses 6.2 dB.**
- **Each filter self-oscillates alone at the same pitch:** 2432-2433 Hz, then 2405 Hz at the last stop. So in this take SPACING at "12 o'clock" was in unison at about 2.4 kHz (*unconfirmed, see the note at the top of this section*). The other takes' offsets at 12 o'clock (+0.1 to +0.41 oct) are therefore most likely where the knob was set by eye. At +5.6 to +5.9 / −5.6 oct over the knob's travel, 0.27 oct is a few degrees (*inference*; it assumes a roughly even law).
- **The 2nd harmonic (130 Hz) sits at −22 dB** already at zero resonance. A triangle has no even harmonics, so some asymmetric drive is present, perhaps from the mixer level.
- The listener hears "a heck of a lot" of bass loss. The measured passband loss is 3-6 dB; the loud resonant peak at 2.4 kHz probably adds to how big the loss sounds (*inference*).

### Earlier takes, mode uncertain (possibly SERIES HP/LP): context only

| Take | What was moved | What was read |
|---|---|---|
| 0004, 1 s | SPACING at centre | one tone, 372 Hz |
| 0003, 62 s | SPACING between its ends and midpoints | 2933 Hz clockwise and 44-48 Hz counterclockwise, read at the time as +2.98 and −3.0 oct against take 0004 |
| 0001, 135 s | CUTOFF through its whole travel | self-oscillation from about 45 Hz (fading in from silence) up to 15.7 kHz; side tones about ⅓ oct either side, 12 dB down |
| 0005, 35 s | CUTOFF at the 2 kHz mark; SPACING from centre to counterclockwise, then clockwise, then back | At 12 o'clock only one tone is present (3322 Hz at the start, 3093-3100 Hz at the end). A second, steady 2857 Hz tone appears only once VCF 1 moves away, so the two filters either lock together or the mode was not LP/LP. VCF 1 reaches 57.9 Hz counterclockwise (used above) and is gone clockwise |

Earlier versions of this section concluded ±3 octaves, then left the counterclockwise end open, then gave at least +5.55 / −4.8 octaves, then ±5.6, then ±5.5 with a half-octave centre offset. That last offset was an artefact of the analysis. Takes 0009-0014 supersede all of these.

**Blind check (2026-11-17).** A second model (Fable) re-measured takes 0005 and 0009-0016 from the audio before reading this section. It confirmed the CUTOFF figures, the SPACING ends (+5.91 / −5.63 oct against VCF 2), the CV slopes and period, and 1.26-1.28 oct/V (*superseded 2026-11-19: about 1.18 oct/V, velcal-t2*). It corrected five things, all checked against the audio and fixed above:
- the unison offset shrinks with cutoff;
- there is no 21.4 kHz ceiling;
- the exponential law compresses above 4 kHz;
- the sine take does not give a reliable volts-per-octave figure;
- in take 0005 the filters merge into one tone at 12 o'clock.

It also noticed, at low confidence, sidebands about ±4.5 Hz and ±9 Hz around a steady tone whenever the other filter is out of hearing range (takes 0009, 0011 and 0012). The silent filter may still oscillate below 20 Hz and couple in. Scripts: not yet in `tools/matriarch/`.

**Note on the tools:** `spacing.py measure` expects one filter per channel. In these takes both filters share one channel, so they were read with a spectral timeline and long FFTs instead.

## Results, 2026-11-19: both filters through the VCF OUT jacks

Takes: (kept locally, not in the repo), each with a `.json` holding the panel state, written before the take and corrected afterwards where the operator said otherwise.
- **Rig:** Model 24 channel 12 = one VCF OUT jack (which one is in each take's json), 13/14 = MAIN L/R; 48 kHz; recorded from Apple Terminal (iTerm records zeros).
- **Source:** the Matriarch's own oscillators (saw, mixer at 9 o'clock, below the drive point), notes over USB MIDI; delay MIX 0, no other cables.
- **Analysis:** per-harmonic levels against a saw (+20·log10 k), complex per-harmonic ratios for the mode sums, peak FFTs for self-oscillation.
- **Knobs** were moved by the operator by hand, cued by count-in notes or spoken cues. Positions are labels, not values (see the step takes below).

### Mode sums (answers filter-matriarch.md open question 5)
- **PARALLEL** (par-t2): MAIN = **1.621 · VCF 1 OUT + 1.630 · VCF 2 OUT**, both positive: the two filters sum **in phase, with equal weight**. The jacks lead MAIN by 42 µs (VCF 1) and 104 µs (VCF 2).
- **SERIES** (lfo-t3): MAIN has the shape of VCF 2 OUT, 6.2 dB above the jack.
- With the PARALLEL sum, the filter not on the jack is derived as (MAIN − 1.630 · tap) / 1.621. The jack lags break this above about 5 kHz; above that only directly recorded tones are used.

### The two filters at RES 0 (PARALLEL)
- **VCF 2 (LP):** 4-pole, about 25 dB/oct above the corner. The passband rises about 3.5 dB as CUTOFF falls.
- **VCF 1 (HP):** the corner follows CUTOFF, with a +3-4 dB bump at RES 0. Below the corner it falls only 3-6 dB/oct, down to a flat leak floor. So VCF 1 OUT does **not** show the 24 dB/oct skirt the manual implies (filter-matriarch.md section 1). Open: where the leak comes from (dry signal or something else), and whether it hides a steeper skirt.
- **A fixed notch at about 160-165 Hz on VCF 1 OUT**, at one CUTOFF value just below the top of the triangle LFO's sweep: −68.6 dB in lfo-t10, −70.8 / −75.8 dB in lfo-t5. It sits at the same frequency for notes 40, 45 and 52, so it does not follow the note. CUTOFF fully clockwise does not reach it (still-cw-40/45). Mechanism open; a leak cancelling the HP output at one frequency would fit (*inference*).

### Resonance and bass loss (VCF 2 OUT, PARALLEL, note 40)
CUTOFF swept from fully clockwise to fully counterclockwise at three RES settings (cutsweep-t1/t2 at RES 0, ressweep-t1 at noon, ressweep-t3 at 3 o'clock); passband compared at the same corner frequencies.

| RES (both) | Passband against RES 0 | Resonant peak |
|---|---|---|
| 0 | reference | none |
| noon | **−10.5 dB** | +8 dB |
| 3 o'clock | **−14.5 dB** | +25 to +39 dB |

- **This contradicts take 0017** (−3.3 dB on VCF 1, −6.2 dB on VCF 2 at full resonance). The setups differ: here PARALLEL, the VCF 2 OUT jack and a saw; take 0017 was STEREO, the main outputs and a 65 Hz triangle at a fixed CUTOFF. Repeated later the same day with take 0017's setup and with one RES knob at a time: the per-filter loss stands, take 0017 stays unexplained (open, unexplained).
- The −14.5 dB is close to a linear ladder's −14 dB at k = 4 (*inference*: RES 3 o'clock is about where self-oscillation starts, MM p. 22, and the peak and the self-oscillation tone were not fully separated in ressweep-t3).

### Self-oscillation range and tracking (RES fully up, oscillators out of the mixer)
- **Range:** from about 21 kHz (VCF 2 20.99 kHz, VCF 1 22.38 kHz) down to about 20 Hz, then silent (ressweep-t4/t5). With CUTOFF at 9 o'clock both are below 20 Hz (track-t1). Agrees with 20 Hz-22.75 kHz on 2026-11-17.
- **The two filters do not track exactly.** With SPACING fixed, VCF 1 against VCF 2 moves from +111 to +343 cents over 7 octaves of CUTOFF (ressweep-t4), and from −115 to +78 cents over 10 octaves (ressweep-t5). Whether the drift depends on the SPACING setting is open (two takes only).

### SPACING (spacing-t2/t3, spacing-steps-a/b, track-t1)
- **Direction:** clockwise moves VCF 1 up (MM p. 21 confirmed; an earlier reading of cutdown-t1 that said the opposite was wrong).
- **Ends:** **+5.63 oct** (VCF 1 at 15.8 kHz against VCF 2 at 319 Hz) and **−5.62 oct** (59 Hz against 2896 Hz). Agrees with +5.91 / −5.63 on 2026-11-17 (both days together: +5.6 to +5.9, −5.6).
- **Law:** linear in octaves (exponential in Hz), about 1.2-1.3 octaves per hour of knob travel (30°). Agrees with 1.1-1.2 oct per 30° on 2026-11-17. The operator finds the knob feels "natural" in octaves.
- **12 o'clock is near unison, but by eye it is not reliable:** +0.16 oct at 7 kHz and about 0 at 50 Hz in track-t1; +0.45 oct at low cutoff in track-cv-t1, with the knob again set to "12" by eye.
- **Setting unison by ear:** about 10 cents (beating at 1.85 Hz) on a quick try, about 1 cent when the operator took time. The knob is made for long strokes, not fine settings.
- **Step takes:** in take A the knob positions were off by about 1.3 hours from the labels; in take B the operator started one tick late. Both were corrected against track-t1 before the law above was read; neither take gives knob positions on its own.

### Agreement with 2026-11-17
CUTOFF range and SPACING ends and law agree. Unison at 12 o'clock agrees only roughly (by-eye settings differ by up to 0.45 oct). The bass loss against resonance does not (open, unexplained).

**Scripts:** `parphase.py`, `cuetake.py`, `sweep.py`, `notch.py`, `bands.py`, `ressweep.py`; not yet in `tools/matriarch/`.

### Remote control of the filters over USB MIDI (2026-11-19, later)
FILTER MODE, CUTOFF and SPACING have no MIDI control, but the KB outputs carry MIDI and can be patched into the CUTOFF inputs. Setup as above: RES fully up, oscillators out of the mixer, ENV AMT at centre, KB TRACKING at minimum, VCF 2 OUT on channel 12; VCF 1 read from MAIN while a note holds the VCA open.

| Out → CUTOFF 2 IN | Follows MIDI? | Take | Result on VCF 2 |
|---|---|---|---|
| MOD WHEEL OUT | **no** | modwheel-t1/t2/t3 | flat at every CC1 value once the wheel's LFO routing is off (t3). In t1/t2 CC1 moved the filters only through the wheel's LFO depth |
| CV OUT (keyboard pitch) | yes | kbcv-t1 | **1.18 oct per octave of notes**, same up and down; note 60 ≈ 0 V (VCF 2 at 343.5 Hz against 346 Hz unpatched); VCF 1 stays put |
| CV OUT, pitch bend | yes | bend-t1 | full bend = **±0.197 oct** (±2 semitones of CV), linear; smooth between notes |
| VEL OUT | yes | vel-t3 | **+5.58 oct from velocity 1 to 127**, about 0.046 oct per step, linear in octaves except the top step (96→127: +1.24 oct, likely the compression above 4 kHz); independent of pitch |

- **Both filters independently** (both-t1): VEL OUT → CUTOFF 2 IN, and CV OUT → ATTENUATOR (fully counterclockwise) → CUTOFF 1 IN. VCF 2 follows velocity only; VCF 1 follows pitch inverted, **−1.19 oct per octave** (gain −1.01). Neither control moves the other filter. MULTITRIG on made no visible difference.
- **If CV OUT is exactly 1 V/oct**, CUTOFF 2 IN is about 1.18 oct/V, 5.6-7.8 % below the 1.25-1.28 oct/V of takes 0014-0016, which rested on the LFO's 10 V spec. Resolved below (velcal-t2): about 1.18 oct/V.
- vel-t1/vel-t2 were recorded with CV OUT still patched (the label said VEL); their `.json` says so.

### Tracking with one shared CV (track-cv-t1)
CV OUT → CUTOFF 1 IN, CUTOFF 2 IN empty, so the normal feeds the same CV to both filters. PARALLEL, SPACING at 12, RES fully up, oscillators out. Notes 24-108 in 2-semitone steps, then back down in 6-semitone steps, 1.2 s each.

| VCF 2 | VCF 1 − VCF 2 |
|---|---|
| 31.8-101.6 Hz (notes 24-42) | +535 to +542 cents, flat |
| 152 Hz | +529 c |
| 345 Hz | +511 c |
| 786 Hz | +486 c |
| 1784 Hz | +456 c |
| 3986 Hz | +413 c |
| 8575 Hz (note 108) | +350 c |

- **The offset shrinks as the cutoff rises:** about −17 cents per octave from 100 Hz to 1 kHz, about −57 c/oct from 4 to 8.6 kHz. Over notes 36-72, VCF 2 moves 1.175 oct and VCF 1 1.161 oct per octave of notes, so VCF 1's CV response is about 1.2 % shallower, more so at the top.
- **No hysteresis:** the down sweep repeats the up sweep within 1 cent at every note.
- Agrees in direction with the 2026-11-17 blind check ("the unison offset shrinks with cutoff"). The hand-turned CUTOFF sweeps earlier the same day (ressweep-t4/t5) gave drifts of +111 → +343 and −115 → +78 cents; whether those read the same direction against frequency was not re-checked, so they are not compared here.
- For Filtær: two cutoffs from one control drift apart by about 0.2 oct across the range; a model that wants Matriarch character could reproduce that with a ~1 % difference in the two filters' exponential scale (*inference*).

### Calibrating the CUTOFF inputs in volts (velcal-t2)
VEL OUT → OSC 1 pitch input, CV OUT unpatched; OSC 1 read from MAIN. Notes 48 and 72 at velocities 1-127 in steps of 16.

| Velocity | OSC 1, note 48 | OSC 1, note 72 |
|---|---|---|
| 1 | 67.3 Hz | 271.0 Hz |
| 16 | 101.2 (+0.589 oct) | 407.7 (+0.589) |
| 32 | 156.4 (+1.216) | 630.4 (+1.218) |
| 64 | 373.4 (+2.472) | 1509.1 (+2.477) |
| 96 | 893.6 (+3.731) | 3621.9 (+3.740) |
| 127 | 2083.7 (+4.952) | about 8.5 kHz |

- **VEL OUT ≈ 0.0393 V per velocity step, linear, about 4.95 V at 127** (assuming OSC 1's pitch input is exactly 1 V/oct). The same at both notes; keyboard tracking alone gives 2.01 oct for 2 octaves of notes.
- **CUTOFF 2 IN ≈ 1.17-1.19 oct/V** below about 3 kHz: vel-t3's 0.046 oct per velocity step divided by 0.0393 V. That matches kbcv-t1's 1.18 oct per octave of notes, which assumed CV OUT is 1 V/oct. Two independent assumptions giving the same figure support both.
- **So the 1.25-1.28 oct/V of 2026-11-17 is probably too high.** It rested on the LFO's specified 10 V peak to peak; at 1.18 oct/V the 12.7-octave sweep implies about 10.8 V (*inference*, not measured).
- velcal-t1 (the take before) had CV OUT patched into an oscillator by mistake and is not used.
- **OSC 2 follows OSC 1's pitch** unless OSC 2 is set to its own control (per the operator, after the take): in velcal-t2 both oscillators carried the VEL offset, so MAIN showed one harmonic series. The calibration uses OSC 1 at velocity 1, which tracks the keyboard (2.01 oct per 2 octaves of notes), so it stands.

### Tracking in LP/LP STEREO (track-cv-stereo-t1)
Same patch and sweep as track-cv-t1, FILTER MODE in STEREO. VCF 1 read from MAIN L, VCF 2 from VCF 2 OUT (MAIN R agrees to the bin). CUTOFF had moved since the PARALLEL take (VCF 2 at note 60: 1242 Hz against 345 Hz), so the two are compared by frequency, not by note.

| VCF 2 | STEREO: VCF 1 − VCF 2 | PARALLEL at about the same VCF 2 |
|---|---|---|
| 107 Hz | +547 c | +535 c (102 Hz) |
| 345 Hz (interpolated in octaves between notes 40 and 42 of the take: 315.4 Hz at +523 c, 362.6 Hz at +519 c) | about +520 c | +511 c |
| 1.24 kHz | +477 c | about +470 c |
| 4.17 kHz | +406 c | +413 c (3.99 kHz) |
| 7.9 kHz | +342 c | about +358 c (interpolated) |
| 16 kHz | +232 c | not reached |
| 21.9 kHz | +159 c | not reached |

- **The drift follows frequency, not the mode or the CUTOFF setting:** two different CUTOFF positions and two modes give the same offset at the same frequency within about ±15 cents. It keeps shrinking faster at the top: about −70 c/oct from 4 to 8 kHz, about −110 c/oct from 8 to 16 kHz.
- **No hysteresis:** the down sweep repeats within 1-2 cents.
- **CV pushes VCF 2 to at least 23.8 kHz** (note 108), above the knob's 22.75 kHz. VCF 1 above about 22 kHz cannot be read at 48 kHz: its note-108 reading (21.9 kHz) is an alias.
- **MAIN rolls off highs against VCF 2 OUT:** the same tone on MAIN R against channel 12 is +4.2 dB at 1 kHz, +2.4 dB at 4 kHz, −0.9 dB at 8 kHz, −5.4 dB at 12.8 kHz, −8.3 dB at 16 kHz and −11.7 dB at 20 kHz. Where in the main path this happens (VCA, output stage) is not known.

### Bass loss against resonance, repeated with take 0017's setup (bass-st-t1/t2/t3)
LP/LP STEREO, one oscillator: a triangle at **65.2 Hz** (note 36), as in take 0017. VEL OUT → CUTOFF 1 IN sweeps both cutoffs the same way in every take (velocity 1-127 in steps of 8, 1.5 s each); CUTOFF at 1 o'clock. RES set by the operator on both filters between takes: 0 (t1), noon (t2), 3 o'clock (t3). The 3 o'clock take's resonant tone shows where the corners sit: VCF 1 / VCF 2 at 1369 / 1044 Hz (velocity 1), 3.5 / 2.8 kHz (32), 8.9 / 7.3 kHz (64), 20 / 17.8 kHz (96).

| Velocity (VCF 2 corner) | Fundamental against RES 0, noon: VCF 1 / VCF 2 | 3 o'clock: VCF 1 / VCF 2 |
|---|---|---|
| 1 (about 1.0 kHz) | −10.9 / −11.0 dB | −15.0 / −14.8 dB |
| 32 (about 2.8 kHz) | −10.7 / −10.9 | −14.9 / −14.8 |
| 64 (about 7.3 kHz) | −10.2 / −10.5 | −14.3 / −14.3 |
| 96 (about 17.8 kHz) | −8.8 / −9.3 | −12.5 / −12.7 |
| 127 | −7.5 / −7.7 | −10.6 / −10.6 |

- **At take 0017's own cutoff (about 2.7 kHz), mode, outputs and source, the bass drops 14.8-14.9 dB at 3 o'clock** and 10.7-10.9 dB at noon, on both filters alike. Take 0017 gave 3.3 dB (VCF 1) and 6.2 dB (VCF 2) at full resonance. The method is therefore not the explanation; take 0017 differs in something else.
- **One difference is visible: drive.** Take 0017's 2nd harmonic sat at −22 dB under the fundamental at RES 0 ("some asymmetric drive"); here it is about −35 dB. A ladder whose resonance feedback saturates on a hot signal loses less bass, so take 0017 may have been driven harder (*inference*; the mixer level of take 0017 was not recorded). Tested below (drive test: ruled out).
- The loss shrinks as the corner rises (about 4 dB less at the top), and the higher harmonics lose less (3rd about 9 dB, 5th about 6 dB at noon).
- The −14.9 dB at 3 o'clock is close to a linear ladder's −14 dB at k = 4 (*inference*: k at 3 o'clock was not measured).

**Drive test (bass-drive-t1/t3):** OSC 1's mixer level at maximum, otherwise as bass-st-t1/t3. The 2nd harmonic rises to −17 to −21 dB under the fundamental at RES 0 (take 0017: −22 dB; normal level: −35 dB). The fundamental rises 5.7-7.0 dB at RES 0 and 7.2 dB at 3 o'clock.

| Velocity | Loss at 3 o'clock, VCF 1: normal / driven | VCF 2: normal / driven |
|---|---|---|
| 1 | −15.0 / −13.4 dB | −14.8 / −13.2 dB |
| 32 | −14.9 / −13.7 | −14.8 / −13.6 |
| 64 | −14.3 / −13.5 | −14.3 / −13.5 |
| 127 | −10.6 / −10.4 | −10.6 / −10.5 |

- **Drive shaves at most about 1.6 dB off the loss**, and only with the corner low. At take 0017's level of drive the loss is still about 13.5 dB, not 3.3-6.2 dB. **Drive does not explain take 0017.**
- What is left: take 0017 itself (see "One knob at a time" below).

**One knob at a time (split-ref, split-res1, split-res2):** STEREO, both CUTOFF inputs empty, CUTOFF at 1 o'clock, OSC 1 at maximum in the mixer (the drive condition) and set an octave lower than in the bass-st takes (32.6 Hz on note 36 instead of 65.2 Hz); as in take 0017, one RESONANCE knob turned at a time.

| Take | MAIN L (VCF 1) | MAIN R (VCF 2) | VCF 2 OUT |
|---|---|---|---|
| RES 1 at 3 o'clock, RES 2 at 0 | **−13.7 dB** | 0.0 dB | 0.0 dB |
| RES 1 at 0, RES 2 at 3 o'clock | 0.0 dB | **−13.8 dB** | −14.7 dB |

(change of the fundamental against both RES at 0; each filter's resonant tone appears only on its own output: 1252 Hz on L, 944 Hz on R.)
- **Each output carries one filter,** and one filter's resonance does not touch the other output. Turning one knob therefore cannot by itself turn a 14 dB loss into 3-6 dB.
- **Take 0017 stays unexplained** (open, unexplained). Its two channels are bit-identical (max |L − R| = 1.2e-7), but that is what the 2026-11-17 chain produces (one output copied to both channels, see "The chain" above), so it says nothing about summing. Candidates not yet tested: the knob positions of its stops, its reference stop, or a patch difference that day.
- Its "the two outputs behave differently" and "each filter self-oscillates alone at the same pitch" readings come from the same take; treat them as unconfirmed until it is explained.
- **Per filter, the loss is about 11 dB at RES noon and about 14-15 dB at 3 o'clock** at normal mixer level (bass-st-t1/t2/t3), and 13.7-13.8 dB at 3 o'clock with the mixer at maximum and the note an octave lower (split-res1/2). Close to a linear ladder.

### Coupling at unison (lock-t2, lock-hold-t1)
STEREO, both filters self-oscillating (RES full, oscillators out), VCF 1 fixed at about 1274 Hz by the knobs, VCF 2 steered by CV OUT → CUTOFF 2 IN (note 64 plus pitch bend, 236 cents per full bend).
- **Sweep (lock-t2):** VCF 2 passes straight through VCF 1 in both directions, linear in bend, while VCF 1 stays at 1272-1277 Hz. No locking wider than about ±3 cents (the resolution of 0.4 s frames).
- **Holds (lock-hold-t1), 3 s Blackman-Harris:** at offsets 0, ±2, ±5, ±10, ±20 and ±50 cents the two tones stay where the CV puts them; at −2 cents they are 1 Hz apart and still separate. **No injection locking down to about 2 cents.**
- **Each tone carries its own side peaks** about 1.6-2.2 Hz and 3.3-3.4 Hz either side, 3-7 dB and 9-17 dB under it, the same pattern on both filters at every offset, including ±50 cents where the other filter is 40 Hz away. So it is a slow wander of each self-oscillation (about ±3 cents at 1.27 kHz), not coupling between the filters. VCF 1 has nothing in its CUTOFF input, so the CV path is not the cause (*inference*: supply or thermal; not checked).
- The blind check's ±4.5 / ±9 Hz sidebands (takes 0009-0012, the other filter out of hearing range) were not seen here; that case was not repeated.
- **For Filtær:** the two filters can be modelled as uncoupled at unison; a small slow pitch wander per self-oscillating filter is part of the character (*inference*).

**High-CV dropout, not reproduced (dropout-t2):** the fast note scan (lock-scan-t1) suggested both filters fell silent around notes 90-96. With 2 s holds read 0.6-1.6 s into each note, VCF 2 tracks cleanly from 4.90 kHz (note 84) to 14.74 kHz (note 102) and back, and VCF 1 stays at 1279-1281 Hz. The scan reading was the one-step timing misread. (MAIN R falls 6-7 dB below VCF 2 OUT at 13-15 kHz, matching the main-output rolloff above.)
