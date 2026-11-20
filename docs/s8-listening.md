# S8 listening gate

`make s8` renders `build/s8/*.wav`: a synthetic drum loop (4 bars groove, 2 bars silence so the tails are heard alone) through the tape engine, dry 1 + wet 0.7, mono, 48 kHz float. Every file is normalised to -1 dBFS peak, so loudness differences between files are NOT audible here. Real loop instead: `S8_IN=/path/loop.wav S8_BPM=97 make s8`.

What the ears already proved (numbers, `make ears`): repeats on the beat within 0.01 ms, exact glides, no clicks on mode/spacing/tempo/nudge changes, bounded runaway. What they cannot judge is taste. The listener should note:

| File | Listen for | Verdict |
|---|---|---|
| `01_sync_{90,120,140}bpm` | 1/8 repeats sit on the groove at each tempo | |
| `02_mode7_even`, `03_mode7_vintage` | three heads at once; even (1:2:3) on the grid vs vintage (1:1.9:2.75, provisional) off it | |
| `04_nudge_minus50`, `05_nudge_plus50` | +-50 cents detune: musical drift or just "late/early"? | |
| `06_hpf_{20,80,300}hz` | feedback HPF: 20 Hz muddy build-up, 80 Hz default, 300 Hz thin repeats | |
| `07_tempo_glide_300ms`, `08_tempo_glide_2000ms` | tempo drops to 0.8x at 2 s: tape-style pitch glide; is 300 ms default inertia right? | |
| `09_drive_0`, `10_drive_12` | record-head saturation on repeats; note drive 0 already gives -34 dB THD at a 0.5 input | |
| `11_runaway_1.1` | intensity above 1: self-oscillation that stays bounded; pulled back to 0.5 at 9 s, must recover | |

Open taste questions the numbers cannot settle:
- Drive: `tanh(g x)/g` keeps small signals at unity but lowers the ceiling (repeats get quieter at high drive). Make-up gain or not?
- Default `fb_lpf` 6 kHz: repeats darken by ~3 dB at 6 kHz per pass. Darker / brighter?
- Mode table is provisional (research item 2); the default mode 4 is heads 2+3.

## Result (2026-11-15)
The listener reported that the S8 set was very good, in both the results and the reasoning behind the tests, and that audio quality was good: approved. Sinc reads and the loop LPF were approved by ear. One finding: feedback accumulated too much in some files (`11_runaway_1.1`, and the multi-head files): fixed by normalising the feedback by head weight (loop gain <= intensity in every mode, ears `s3_loopgain_mode4/7`) and a pull-back in file 11. Files rendered before that fix ran mode 4 at about 2x and mode 7 at about 3x the nominal loop gain.

## Parameters and why (all: 48 kHz, mono, dry 1 + wet 0.7, synthetic loop 4 bars groove + 2 bars silence)
- Drum loop (`drumloop.py`): kick on 1, the "and" of 2 and on 3; snare on 2 and 4; 8th hats. Sharp transients show timing errors at once; the off-beat kick shows whether 1/8 repeats land in the groove or smear it; the 2 silent bars let the tails be judged alone. Deterministic (seed 1234), so every render is comparable.
- 1/8 division on head 1 everywhere: the most common echo setting and fast enough that a timing error of a few ms is audible against the hats.
- `01` 90/120/140 BPM, mode 1, defaults: the tempo range of the gate; one head so sync is judged without overlap.
- `02`/`03` mode 7, even vs vintage spacing: all heads at once; even lands on the 1/8 grid (1:2:3 = 1/8, 1/4, 3/8), vintage (provisional 1:1.9:2.75) deliberately off it, to hear if "off" sounds vintage or wrong.
- `04`/`05` nudge -50/+50 cents: a quarter-tone, big enough to hear the drift against the grid (~3 % of the delay time), small enough to be a usable setting.
- `06` HPF 20/80/300 Hz at intensity 0.8, mode 4: high feedback so the bass build-up has time to show; 20 = nearly off (mud), 80 = default, 300 = thin.
- `07`/`08` tempo to 0.8x at 2 s, mode 7, inertia 300 ms vs 2 s: a 20 % drop is a clear glide (about -3.9 semitones on the repeats) on all three heads; the two inertias bracket "quick knob" and "slow motor".
- `09`/`10` drive 0 vs +12 dB at intensity 0.9: saturation builds up over repeats, so high feedback shows its character.
- `11` intensity 1.1, then 0.5 at 9 s: runaway must stay bounded and come back when pulled back.
