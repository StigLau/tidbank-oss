---
name: matriarch-bench
description: Measuring a Moog Matriarch on the bench (delay and filters) as the hardware reference for TidBank's BBD engine and Filtær - the rig (Tascam Model 24 + USB MIDI), tools/matriarch/delay.py and spacing.py, panel recipes, what is already measured, and the traps that produced wrong numbers on 2026-11-17. Use before planning or analysing any hardware take, before quoting a Matriarch number, and before changing a BBD/filter profile to match the hardware.
---

# Matriarch bench

The operator plays the panel; the agent sends MIDI, records, analyses and writes the results down. The protocols and the results
live in the docs; the rig and the traps live here:
- Delay: `docs/research/matriarch-measurement.md` (section 0b = this rig; "Results 2026-11-17").
- Filters: `docs/research/matriarch-spacing-measurement.md` (Results; blind check).
- Manual (printed page numbers): the operator's own PDF, never committed.
Raw takes are kept locally, not in the repo (WAV + `.json` settings + `.result.json` per take, archived as FLAC). Take ids, settings and results are in the docs.

## Rig (works, 2026-11-17)
- **Record on the Tascam Model 24** (CoreAudio "Model 24", 48 kHz). `delay.py` records `--in-ch 13,14` by default; any
  input works, 1-based. Ask the operator which jack is on which channel and confirm it with one take: 13/14 was misread once.
- **Play the Matriarch itself over USB MIDI** (port "Moog Matriarch"): notes are the test signal, CC12 = TIME, CC13 =
  SPACING, CC88 = PING PONG. No audio output from the computer is needed.
- **The Digitakt did not work** as an interface (it did not pass its inputs or play, 2026-11-17). Do not retry it
  without a reason.
- Brightness (Global 0.9) **cannot be trusted from the keyboard**: twice it did not take. Set it in the web editor
  https://mark.reid.name/matriarch-editor/ and have the operator read the mode there before the take.

## Patches that answer which question
| Question | Channels | Panel |
|---|---|---|
| Absolute delay time, response, per-pass loss | MAIN OUT L (dry) + rear DELAY OUT L (wet) | MIX fully ccw (main = dry VCA 1 = Delay 1's input) |
| Delay 1 against Delay 2 (SPACING) | MAIN OUT L + R | MIX fully cw (main L = Delay 1, R = Delay 2); values are t_d1 - t_d2 |
| Ping-pong | MAIN OUT L + R | MIX fully cw, dead cable in INPUT 2 so only Delay 1 gets the note; with the same note in both delays on and off look alike |
| Timing (any) | - | NOISE only (osc off), short notes, FEEDBACK min |
| Response | - | NOISE, **CUTOFF fully cw** (the VCF sits before the delay; at 12 it hid everything above 2 kHz) |
| Pitch effects (TIME moves under repeats) | - | one square osc, NOISE off, FEEDBACK 1 o'clock |

- **Remote filter control** (2026-11-19): CV OUT and VEL OUT follow USB MIDI notes, velocity and pitch bend; MOD WHEEL OUT
  does not follow CC1. VEL OUT -> CUTOFF 2 IN moves VCF 2 (about 0.046 oct per velocity step); CV OUT -> ATTENUATOR fully
  ccw -> CUTOFF 1 IN moves VCF 1 inverted by note. Turn the wheel's LFO routing off first, or CC1 adds an LFO to cutoff.

## Commands
`uv run --with numpy --with scipy --with sounddevice --with mido --with python-rtmidi tools/matriarch/delay.py ...`
`take NAME` (one setting), `sweep NAME --cc time|spacing --values 0:16383:17` (MSB steps), `seq NAME --preset
slew|ramp|wobble|echoes` (TIME under change, ping-pong; slew needs `--ch ref,wet1`), `analyse <wav>`, `selftest`
(run it after every analysis change). Table with panel per command: matriarch-measurement.md section 0b.
`spacing.py measure` expects one filter per channel.

## Known (measured 2026-11-17; numbers in the docs)
- TIME 41.65-404.18 ms, linear in CC12 MSB (42.04 + 2.847 ms/step). Not 35-780 ms as the manual says.
- SPACING adds milliseconds to Delay 1 (nearly the same at TIME min and 203 ms; TIME max measured 2026-11-19, below), clamped near the
  TIME minimum (45.0 ms at TIME 203). Inferred then: TIME max + SPACING max is about 764 ms (measured 2026-11-19: 768.7 ms), close to the
  manual's 780 (p. 32). The plugin's Spacing stays a ratio (decided 2026-11-18).
- Filter in the delay: Bright -3 dB at 2.31-2.35 kHz over TIME 43.6-202.9 ms, 2.09 kHz at 403.6 ms; Dark 1.41 kHz,
  measured at TIME 203 ms only; both about -50 dB/oct; each repeat passes it again. Path does not invert.
- A TIME change re-pitches what is in the loop by t_old/t_new and the repeats keep that pitch. TIME settles
  < 0.45 s (the first note after a change may be off by up to 30 ms: open).
- 2026-11-19: SPACING at TIME max gives Delay 1 50.9-768.7 ms, Delay 2 stays put; CC values are steps (2.6-3.2 ms,
  mean 2.86, per TIME step), a jump lands within about 1-10 ms (inferred, BBD flight model); sweeping re-pitches by the slope; no clock tone above about
  -120 dBFS in 200 Hz-23 kHz. Numbers: `matriarch-measurement.md`, Results 2026-11-19.
- Open: clock rate, level/compander, 54 Hz after a wobble near runaway.

## Traps (each one gave a wrong number before it was caught)
- **Scheduled times are not recording times.** The audio stream starts 0-200 ms after the script's clock. Find the
  notes in the recorded dry channel; never use the schedule to cut windows. Bit again 2026-11-19: a 0.9 s note scan
  read every step one note late and put the unison crossing 3 semitones off (take lock-scan-t1).
- **Periodic notes are ambiguous.** A steady square correlates at every period, and |correlation| at every half period
  with inverted sign (3.75 ms at 130 Hz). Take a coarse lag from envelopes, refine within +-1.5 ms, decide polarity
  once per take - or use noise, which settled it (square takes read about 45.5 ms at CC 0; noise gave 43.6 ms at MSB 0
  and 41.65 ms at the knob minimum).
- **CC LSB (44/45): CC12 followed at once by CC44 is dropped** (the 2026-11-17 sweep "stuck at 222.8/225.6 ms";
  explained 2026-11-19, take `delay-lsb-t2`). The LSBs do work and span one MSB step (TIME is 14-bit):
  leave at least 2 ms between CC12 and CC44 (`delay-lsb-t3`), or send the MSB only.
- **A knob the operator touches overrides the CC**, and a knob far from the CC value can still bend the result (SPACING knob at
  max under CC13 = 64 gave a late +7.4 ms step). Ask the operator to park knobs near what the CC sets, and to not touch them.
- **Knob positions "by eye" are approximate**: 12 o'clock varied by 0.4 oct on the filter and by several ms on SPACING.
  Use CC where the parameter has one; otherwise treat clock positions as labels, not values.
- **State the state.** Before each take, write down what changed (MIX, FEEDBACK, mode, where the last CC left TIME).
  TIME left at CC 0 by the previous test made two ping-pong takes invalid. Mark invalid takes in their `.json`; keep them.
- **Confounds upstream**: the VCF (CUTOFF 12) low-passed the noise before the delay, so wet/dry looked flat to 18 kHz.
- **Interface lines are not the synth**: compare with an unconnected input (8-9 kHz, 16 kHz lines on the Model 24).
- **Re-pitch is smaller than the harmonic spacing at a high harmonic.** With note 60 sounding at 130.8 Hz, harmonics sit
  150 cents apart near 1.4 kHz, so a -80 cent shift reads as a neighbour harmonic. Play a high note (note 96, about
  1 kHz) and track its fundamental; a 10 Hz note train adds +-18 cent comb lines. Cut the band below f0 and the
  sidelobes look like tones.
- **Find the click-to-recording offset in each take.** It was -50 to -105 ms on most takes and -514 ms on one, the same evening;
  an onset window built on the schedule read nonsense twice (2026-11-19). Average the envelopes over the static clicks.
- **One click's echoes die fast.** At FEEDBACK 1 o'clock they lose about 4 dB per pass (2026-11-17 read 3.2 dB at 1 o'clock; knob by eye) and reach the -99 dBFS
  oscillator bleed within about 2 s; to read pitch during a move, keep clicking through it (`delay-pp-t1` vs `-t2`).
- **Pitch from a 0.1 s peak track is coarse**: it cannot follow a 6-12 Hz wobble and may pick a low tone near runaway.
  Say so, and keep the WAV for the human.
- **Runaway feedback**: FEEDBACK just below 2 o'clock can run away during a wobble; the operator may have to lower it.
- **Some terminal apps record exact zeros** from the Model 24 (all channels, even the noise floor; iTerm did, Apple Terminal did not). All
  zeros means no stream, not a silent synth (2026-11-19).
- **Ask which VCF OUT is on channel 12 before every take**, and write it in the `.json`. On 2026-11-19 the jack had changed unnoted
  and a take was first read as the other filter. VCF OUT jacks are pre-VCA: they carry sound with no note held.
- **Cue notes are silent when the oscillators are out of the mixer** (self-oscillation takes). Speak the cues with
  `say` instead, and check the operator hears the voice before the take.
- **Hand-turned knob positions drift**: in one step take every position was about 1.3 hours off, in another the operator started
  one tick late. Record a reference (a fixed position held for a few seconds) in the same take and correct against it.
- **Peak search with an exclusion window makes edge artefacts**: a 2 % exclusion shows up as tones exactly ±34 cents
  (or a fixed Hz offset) from the main one. Check that a second tone is not sitting on the window edge.
- **Confirm a repatch happened before the take.** "Ready afterwards" meant "not yet": vel-t1/t2 ran on the old cable.
  A take with a built-in discriminator (vary the thing the old patch would follow) catches it.
- **Deriving the off-jack filter from MAIN fails high**: MAIN - tap uses the PARALLEL weights and needs the jacks' lag
  (42/104 µs), so it is unusable above about 5 kHz. Use directly recorded tones up there.

## Validating a claim before it goes in a doc
1. Self-test first (`delay.py selftest`, `spacing.py selftest`): synthetic takes with known answers, including the
   trap that was just found. If it cannot fail on the trap, it does not test it.
2. Repeat: two takes or a CC sweep that agree within the stated spread; quote the spread.
3. Change the method when a number looks odd: noise vs square, onset vs correlation, another patch. A second method
   that agrees is the evidence; a first method alone is a reading.
4. For a whole result set, a blind re-measure by another model from the audio, before it reads the doc (the filter
   doc's blind check corrected five findings).
5. Write what is measured, what is inferred, and what is open, separately. Correct wrong numbers in place and say so
   (the filter SPACING range changed five times before it held).
6. Panel modes the operator is unsure of ("I think I set dark") are recorded as unsure, and confirmed before the number counts.
