# Matriarch delay: measurement protocol

Status: Rig A (section 0b) run 2026-11-17, results below; sections 2-5 on the calibrated rig not run (clock, compander and level). Sections 3-5 are based on Gemini Deep Research (2026-11-16), with corrections; sections 0-2 and 6 are new. Reviewed by a second model (Fable) on 2026-11-16 against the Matriarch manual (https://www.adorama.com/col/productManuals/MGODMATR01.pdf, printed page numbers) and the review's fixes are applied.

The expected values come from `bbd-research.md` 8.1 and are mostly inferred from the Moog 500 Series manual. What we measure here replaces them in the Matriarch profile (d21).

## 0. Setup
- **Interface:** 192 kHz, 24 bit. The jacks are 10 Vpp (manual pp. 34, 56), which is about +13.2 dBu. Check that the interface input and output reach that, or pad them.
- **Calibrate:** send a 1 kHz sine and measure Vrms at the cable end with a meter, then do the same for the input. Write down dBFS -> V for both. All levels below are given in volts at the jack.
- **Patch:**
  - Interface out -> front INPUT 1 (or INPUT 2). These replace the VCA 1/2 feed (p. 34).
  - Rear DELAY OUT L (Delay 1) and R (Delay 2) -> interface in. These outputs are always 100 % wet (p. 56), so MIX doesn't matter.
- **Defaults unless stated:**
  - SPACING centred, then measure that t_d1 = t_d2 (SPACING offsets Delay 1 against Delay 2, p. 32).
  - PING PONG off (it cross-feeds the lines, p. 33).
  - TIME 1/2 IN unpatched.
  - SYNC/TAP off: TIME LED red, not synced (pp. 32-33).
  - FEEDBACK at minimum.
- **TIME positions:** min, 9:00, 12:00, 3:00, max. Photograph the knobs every run. For fine steps, set TIME over MIDI (CC12/44, pp. 73-74).
- **Brightness:** this is not a CC. It is Global 0.9: hold HOLD + SYNC ENABLE, press key G#1 (9 blinks), then C0 = Dark, D0 = Bright. The default is Bright (pp. 61, 63). On 2026-11-17 the keyboard route did not take twice; the web editor https://mark.reid.name/matriarch-editor/ did. Confirm the mode there before a take.
- Keep the raw recordings out of git. Write down where they are stored.

## 0b. Rig A: Model 24 + USB MIDI (`tools/matriarch/delay.py`; the Digitakt did not work)
A lighter rig that needs no calibration: the Tascam Model 24 (USB audio, 48 kHz, one input per mixer channel) records,
and the script plays notes and sets TIME, SPACING and PING PONG over the Matriarch's own USB MIDI (port "Moog Matriarch"). The Matriarch's own
sound is the test signal. It measures times, ratios and spectra only. Not covered by Rig A: volts at the jack,
compander thresholds, THD against level, and anything above 24 kHz (clock lines above it fold or vanish).

- **Patch (MIDI source, the default):** the Matriarch has no VCA out jack, so the dry reference is the main out.
  - MAIN OUT L -> Model 24 channel 13 (IN L), with MIX fully counterclockwise: the dry VCA 1 signal, which is Delay 1's input (p. 33).
  - Rear DELAY OUT L (Delay 1, always 100 % wet, p. 56) -> channel 14 (IN R). Nothing in INPUT 1.
  - Delay 1 against Delay 2 (SPACING, matching): DELAY OUT L -> IN L, DELAY OUT R -> IN R, and `--ch wet1,wet2`.
  - DELAY OUT is 10 V p-p with no level control: if IN R clips, lower the oscillator level in the Mixer.
- **Sound:** one oscillator (saw) or noise, filter open, amp envelope attack 0, short decay, sustain 0.
- **Interface:** `--device "Model 24" --in-ch 13,14` (the defaults). The Digitakt did not pass its inputs or play on 2026-11-17.
- **`--source audio`** plays the signal on the interface OUT L instead (-> MULT -> INPUT 1, MULT -> IN L). Not working on
  2026-11-17; kept for an interface that plays.
- **Digitakt (tried first, did not work):** checked 2026-11-17, notes sent over MIDI reached its USB inputs (-20 dBFS
  peak), but IN L and IN R read the same before patching, and audio played from the computer was not heard. We moved to
  the Model 24.
- **Check the rig first:** one `take` with `--count 2`; IN L should show the notes and IN R the delayed notes.
- **Commands** (`uv run --with numpy --with scipy --with sounddevice --with mido --with python-rtmidi tools/matriarch/delay.py ...`):

| Measurement | Command | Panel |
|---|---|---|
| t_d at a knob position | `take t-1200 --signal clicks --note "TIME 12:00"` | FEEDBACK min |
| t_d against CC12 | `sweep time --cc time --values 0:16383:17` | FEEDBACK min, SYNC off |
| SPACING law | `sweep sp --cc spacing --values 0:16383:17 --ch wet1,wet2` | TIME fixed |
| Loss per pass | `take fb-0900 --signal clicks --period 4 --note "FEEDBACK 9:00"` | TIME short (repeats fit in the period) |
| Response, Bright/Dark | `take resp-bright --signal noise --note "Bright, TIME 12:00"` | FEEDBACK min |
| Noise and clock lines | `take quiet-max --signal silence --note "TIME max"` | no notes; oscillator levels down |
| TIME settling | `seq slew --preset slew --lo 0 --hi 127 --ch ref,wet1 --in-ch 13,12` | MIX ccw, DELAY OUT L on ch 12, NOISE, FEEDBACK min |
| Repeats under a TIME ramp | `seq bend --preset ramp --lo 32 --hi 96 --secs 2` | square note, FEEDBACK 1, MIX fully cw |
| TIME wobble | `seq wob --preset wobble --rate 2 --secs 3 --pingpong on` | square note, FEEDBACK 1, MIX fully cw |
| Ping-pong | `seq pp --preset echoes --lo 64 --pingpong on` | dead cable in INPUT 2, MIX fully cw, NOISE, FEEDBACK 12 |

- **Output:** each take writes `<name>.wav` (played, IN L, IN R; 32-bit float), `<name>.json` (signal and settings)
  and `<name>.result.json` to a directory kept locally, not in the repo. `delay.py analyse <wav>` re-runs the
  analysis. `delay.py selftest` checks the analysis on synthetic takes.
- **Limits of the analysis:** t_d is the cross-correlation peak of each burst, interpolated to sub-sample. The
  response is wet/reference in 1/6-octave bands, normalised to 200-800 Hz. Repeat levels are relative to the first
  repeat, and only multiples of t_d count as repeats.

## 0c. How a bench session runs (the operator at the panel, the agent at the computer)
This is how the 2026-11-17 session worked; it is the template for the next one.

**Roles.** The operator patches and turns the knobs that have no MIDI (FEEDBACK, MIX, CUTOFF, mixer levels), and says what was set.
The agent plays the Matriarch over USB MIDI, records the interface, analyses each take and reports the numbers, then asks
for the next panel change. Nothing is played from the computer: the Matriarch's own voice, triggered by MIDI notes, is the
test signal.

**Setup, once:**
1. Matriarch USB to the computer. It shows up as the MIDI port "Moog Matriarch"; no driver. `delay.py devices` lists the
   audio devices and MIDI ports.
2. Model 24 over USB as the CoreAudio device "Model 24", 48 kHz. Each mixer channel is a numbered input (`--in-ch`,
   1-based). Patch the Matriarch outputs (section 0b) into two channels' line inputs; a third channel (we used 12) can
   carry a second wet output.
3. Ask the operator which jack is on which channel and check it with one take: notes on the dry channel, delayed notes on the
   wet one. Channel roles were swapped once by a misread label.
4. Global settings (Delay Filter Brightness and the like) are set in the web editor https://mark.reid.name/matriarch-editor/
   and read back there; the keyboard route (HOLD + SYNC ENABLE) did not take, twice.

**Each take:**
1. The operator states the panel (what changed since the last take). The agent writes it into `--note`, so it lands in the take's
   `.json`.
2. The agent runs one `take`, `sweep` or `seq` (section 0b table); the operator says "go" when the panel is set and keeps hands
   off the knobs the CC sets.
3. The agent reports the numbers and anything odd, and proposes the next setting. A take made with a wrong setting is
   marked invalid in its `.json` and kept.

**What did not work:** the Digitakt as the interface; the computer's audio output as the signal (`--source audio`);
global settings from the keyboard. The CC LSB (CC44/45), listed here until 2026-11-19, works: send it 2 ms or more
after CC12 (Results 2026-11-19). The traps behind the wrong numbers, and how each was caught,
are in the `matriarch-bench` skill.

**Kept out of git:** the takes (kept locally, not in the repo) and the Matriarch manual PDF.

## 1. Delay time vs TIME
- **Signal:** single clicks 1 s apart, both lines.
- **Measure:** t_d at each TIME position. Check that the ends are 35 and 780 ms.
- **Fine sweep:** step TIME in small CC12 steps and look for jumps in t_d or in the noise character. The 22:1 range is wider than one 500 Series range (that unit uses two), so the Matriarch may switch stages or ranges inside it. That switching is an inference, not a fact.
- **Longer delays:** tap tempo can set delays beyond 780 ms (p. 32 tip), which gives lower f_clk.

## 2. Clock frequency and stage count
- **Signal:** the noise runs from section 5, or a low-level 1 kHz tone.
- **Measure:**
  - f_clk from the clock line in the wet spectrum (also check 2 f_clk).
  - N = 2 f_clk t_d.
- **Expect N = 8192.** That gives:

  | t_d | f_clk |
  |---|---|
  | 780 ms | 5.25 kHz |
  | 400 ms | 10.2 kHz |
  | 35 ms | 117 kHz (above the MN3008's 100 kHz maximum) |

- **Limits:**
  - A 12th-order low-pass at 2.6 kHz or below may bury the clock line everywhere except the longest delays (tap tempo helps there).
  - Above 96 kHz (t_d below about 43 ms for N = 8192) the line can't be seen directly. Use the image products k f_clk +- f0 of a test tone, or report "not measurable".
  - Write down which TIME positions actually gave an N.

## 3. Filters: order and cutoff, Bright vs Dark
- **Why a plain sweep misleads.** The filters (and the BBD hold) sit between the 2:1 compressor and the 1:2 expander. A steady sine therefore comes out at |H|²: the attenuation in dB doubles. A plain ESS would read the filters' -1.5 dB point as "-3 dB", and the slope could look like 144 dB/oct. This is an inference from the 8.1 chain and the 8.6 law.
- **Signal:** a 10 s ESS (20 Hz-20 kHz) at least 20 dB below a fixed 100 Hz pilot tone. The pilot sets the rectifier level and so holds the expander gain still. Deconvolve against the probe only.
- **Cross-check:** a sweep without the pilot, with its dB halved, should match.
- **Matrix:** Bright / Dark x TIME (min, 12:00, max) x 2 pilot levels.
- **Read:** the -3 and -6 dB points, and the slope between fc and 2 fc.
- **Clock-tracking check.** Compare the min, 12:00 and max curves. If they overlay, the filters are fixed. If not, the filter follows TIME (on the 500 Series, the 0.5x / 1.0x switch also sets Bright / Dark).
- **Hold droop.**
  - Candidates: T_h = 1/f_clk (summed outputs) or 1/(2 f_clk) (one output). At f_clk 5.25 kHz and 2.6 kHz they give -3.84 dB and -0.89 dB.
  - At 780 ms, 2.6 kHz is the BBD's Nyquist frequency (2625 Hz), so droop, filter and images can't be told apart there.
  - Fit the filter at min TIME, where the droop is negligible. Show that the filter is fixed. Only then fit T_h from the long-delay curves against that reference.
- **Expect:** -3 dB at 2.6 kHz (Bright) and 1.65 kHz (Dark) (500 Series, p. 21). Two 6th-order filters in cascade give a 72 dB/oct asymptote; the noise floor probably cuts it short.

## 4. Compander, limiting, nonlinearity
- **Burst.**
  - Signal: 500 Hz, stepping from low to high level (+34 dB, 200 ms), then back down (500 ms).
  - Read: the wet envelope (overshoot, recovery), and the noise floor decaying after the burst ends. The noise decay shows the expander's time constant better than the envelope does.
  - Reference: the DM-2's tau is 2.2 ms; the Matriarch's is unknown.
  - The NE570 expander is a plain 1:2 expansion, not a gate.
- **Limiting.** The manual describes no limiting LED; the only delay LED shows the sync state (pp. 32-33).
  - Signal: a stepped 1 kHz sine, from low level up to the 10 Vpp jack limit, in 2 dB steps.
  - Plot output against input level. Read the limiting threshold and ratio, and where clipping starts.
- **Nonlinearity.** Stepped-sine THD (H2, H3, H5) against level at 100 Hz, 500 Hz and 1 kHz. ESS harmonics mix BBD distortion with compander ripple at low frequencies, so don't use them for this.
- **Feedback per pass.**
  - Set FEEDBACK by measured decay (a repeat about 6 dB below the one before), not by knob position: above about 2 o'clock it self-oscillates (p. 33).
  - Signal: one 100 ms burst of pink noise (50 Hz-5 kHz) at mid level, TIME at 400 ms.
  - Take Y(k+1)/Y(k) for repeats 1-4 and compare it with the single-pass |H| from section 3. Per the 500 Series manual, each repeat should pass both filters.
- **Self-oscillation.** FEEDBACK past 2 o'clock. Read the waveform and harmonics:
  - mostly odd: symmetric clipping;
  - odd and even: asymmetric BBD saturation;
  - a clean tone that pumps slowly: gain control.

## 5. Noise and clock feedthrough vs clock rate
- **Idle.** Grounded input (100 Ohm into INPUT 1), 10 s at each TIME position, Bright and Dark.
- **Expander open.**
  - Signal: a 60 Hz sine at mid level, same captures.
  - Filter: 4th-order high-pass at 250 Hz. Notch 60 Hz and its harmonics, and the images f_clk +- 60 Hz.
  - Report the clock lines f_clk and 2 f_clk on their own, in dB re the signal.
- **Report per f_clk:**
  - RMS noise 250 Hz-20 kHz, idle and expander open.
  - Clock line level, Bright and Dark.

## 6. Stereo
- **L/R matching:** t_d, f_clk, |H| and noise of Delay 1 vs Delay 2 at the same settings.
- **Spacing:** t_d1 and t_d2 against SPACING.
- **Ping-pong:** a click into INPUT 1. Where and when does each repeat appear?
- **Sync:** with SYNC lit, TIME becomes clock divisions (pp. 32-33). That MIDI Clock In (Global 1.5) leaves TIME alone with SYNC off is an inference; check it once.

## Results 2026-11-17 (Rig A on the Model 24, ch 13 = MAIN OUT L dry, ch 14 = DELAY OUT L)
Takes: kept locally, not in the repo. FEEDBACK min, SPACING 12, MIX fully ccw,
SYNC off, PING PONG off. Test sound: NOISE at 9 only, short notes over USB MIDI (square-wave takes are ambiguous by
half a period, see `delay.py`).

- **TIME range is 41.65 to 404.18 ms**, not 35 to 780 ms (p. 32). Knob fully ccw 41.63-41.69 ms over 6 notes; fully
  cw 404.17-404.26 ms over 15 notes (take `noise-knob-min2max`). The 780 ms may need tap tempo or CV (p. 32 tip): not checked.
- **TIME against CC12 (MSB only) is linear:** t_d = 42.04 + 2.847 ms per MSB step, residual at most 0.025 ms from MSB 7
  to 127 (square-wave sweep); the noise sweep repeats it within 0.05 ms at 12 of 17 steps. MSB 0 gives 43.6 ms, 1.5 ms
  above the line and 2 ms above the knob minimum. MSB 127 gives 403.6 ms, the knob maximum within 0.6 ms.
- **CC44 (LSB) misbehaves:** sent after CC12 it pinned the sweep at 222.8/225.6 ms; sent alone, a raised LSB shortened
  the delay by more than one MSB step. `delay.py` sends the MSB only. **Superseded 2026-11-19** (Results 2026-11-19,
  `delay-lsb-t3`): sent alone, the LSB moves the delay by one MSB step at most, monotonic and longer with a higher LSB;
  the pinning is CC12 being dropped when CC44 follows back-to-back.
- **Polarity:** the delay path does not invert (noise takes).
- **Knob 12:00 = 223.6-224.8 ms** (set by eye, three takes), which is MSB 64 on the CC line.
- **FEEDBACK (by eye, TIME 202.9 ms, take `fb-steps`):** loss per pass 9 o'clock about -14.5 dB (gain 0.19), 12 about
  -5.5 dB (0.53), 1 about -3.2 dB (0.69), just below 2 about -0.65 dB (0.93); past that it self-oscillates (p. 33 says
  above about 2 o'clock). The loss per pass is steady over 4 passes at each setting.
- **Response (noise, CUTOFF fully cw):** flat to about 1 kHz, -3 dB at 2.3-2.35 kHz, -6 dB at 2.56-2.58 kHz, then about
  -50 dB per octave (-14 dB at 2.9 kHz, -49 dB at 4.1 kHz). It barely moves with TIME: -3 dB at 2.31 kHz (43.6 ms),
  2.35 kHz (202.9 ms), 2.09 kHz (403.6 ms). So within this range it is a fixed filter, not one that follows the clock.
- **Each repeat passes the same filter again** (FEEDBACK 12): pass 2 against pass 1 is -5.2 dB at 250 Hz, -7.2 dB at
  2 kHz, -16.4 dB at 3 kHz; pass 3 doubles it.
- **Bright vs Dark (TIME 203 ms, CUTOFF fully cw):** Bright -3 dB at 2.35 kHz, -6 dB at 2.58 kHz; Dark -3 dB at 1.41 kHz,
  -6 dB at 1.58 kHz (-22.8 dB at 2 kHz). Same slope, about -50 dB per octave; the corner moves down about 0.74 octave.
  With FEEDBACK 12 on Dark, pass 2 against pass 1 is -4.3 dB at 250 Hz, -5.2 dB at 1 kHz, -19.5 dB at 2 kHz. Dark also
  read 0.37 ms longer (203.30 against 202.93 ms). Setting it from the keyboard (HOLD + SYNC ENABLE, G#1, C0) did not
  take, twice, so takes `resp-dark*` and `fb12-dark` are Bright. Dark was set with the web editor
  https://mark.reid.name/matriarch-editor/ and confirmed there (takes `resp-editor`, `fb12-editor`). All other takes
  are Bright, the default.
- **SPACING over CC13 (MSB only; TIME CC12 MSB 56, Delay 2 = 202.9 ms; MIX fully cw so MAIN OUT L = Delay 1, R = Delay 2;
  take `sp-spacing*`).** Values are t_d1 - t_d2, three notes each, all within 0.002 ms:

  | CC13 MSB | 0-23 | 31 | 39 | 47 | 55 | 63 | 71 | 79 | 87 | 95 | 103 | 111 | 119 | 127 |
  |---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
  | t_d1 - t_d2 (ms) | -157.9 | -148.5 | -106.7 | -68.6 | -33.8 | -2.0 | +29.6 | +64.0 | +101.6 | +143.0 | +188.7 | +239.6 | +296.3 | +360.2 |

  Delay 1 then runs from 45.0 ms (clamped: MSB 0-23 all read 45.0, near the TIME minimum) to 563 ms, 2.78 times
  Delay 2 and longer than TIME's own maximum (404 ms). Zero is near MSB 64; the SPACING knob at 12 by eye read +3.33 ms.
  The step per 8 MSB is smallest at the centre (31.5-31.9 ms) and grows to both sides (41.8 ms at 31-39, 63.8 ms at
  119-127), so the law is neither linear in ms nor a constant ratio. Not checked: whether it scales with TIME, and
  whether Delay 2 stays put (assumed from p. 32; checked 2026-11-19: it does).
- **SPACING adds milliseconds; it does not scale TIME.** Same sweep with the TIME knob fully ccw (Delay 2 = 41.65 ms,
  take `sptmin-spacing*`): MSB 0-63 all read -0.03 ms (Delay 1 cannot go below the minimum), then +25.2, +59.6, +97.2,
  +138.6, +184.3, +235.2, +291.9, +355.8 ms at MSB 71-127. That is the 203 ms curve minus 4.4 ms at every step (4.38-4.42).
  So t_d1 = TIME + f(SPACING), with f nearly independent of TIME. Inferred, not measured: TIME max (404 ms) plus SPACING
  max (+360 ms) gives about 764 ms, close to the manual's 780 ms (p. 32), which would then be Delay 1's maximum.
  (Measured 2026-11-19: 768.7 ms over MIDI, Results 2026-11-19.)
- **TIME change, slew (take `slew-time`, ch 13 dry, ch 12 DELAY OUT L, SPACING CC13 64 with the knob at max):** a CC12
  jump 0 -> 127 is in place within 0.45 s (44.04 -> 404.08 ms), but 2-3 s later t_d stepped again to 411.45 ms; after
  127 -> 0 it settled at 47.75 ms, not 44.04. Repeated with the SPACING knob at 12 and no CC13 (take `slew-time-sp12`):
  50.55 ms (10 notes, within 0.01 ms) -> 414.32 ms from the first note 0.45 s after the jump -> 50.55 ms again from
  0.45 s after the jump back. No late step and no offset, so they came from the SPACING knob at max under CC13. TIME
  settles within 0.45 s. (Delay 1 is 7-11 ms above the CC12 line here because the SPACING knob, set by eye, is a little
  past centre.)
- **TIME change under live repeats (take `bend-time`, FEEDBACK 1 o'clock, square at 130.9 Hz, SPACING knob 12):** CC12
  MSB 32 -> 96 over 2 s (133 -> 315 ms) lowers the repeats smoothly to 57.6 Hz, and they keep that pitch afterwards
  (2.5 s, until they die out). 96 -> 32 on a fresh note raises them to 299 Hz and they stay there. The pitch ratio (0.44
  and 2.29) follows the delay ratio (0.42 and 2.37) within 4-5 %. So what is in the delay comes out scaled by
  t_old/t_new and keeps that pitch round the feedback loop: the behaviour of a clocked buffer (BBD), and also of tape or
  a variable-rate digital delay, so it does not tell which.
- **PING PONG (CC88; MIX fully cw, ch 13 = MAIN OUT L = Delay 1, ch 14 = R = Delay 2; FEEDBACK 12; TIME CC12 MSB 64;
  dead cable in INPUT 2 so only Delay 1 gets the note; takes `pp-*-in2dead`).** Off: echoes on L only, about every
  235 ms, R silent. On: the echoes alternate L, R, L, R ... (8 passes visible, 228-237 ms apart), the first on L. Level
  per pass about -5.7 dB on average; the L -> R steps looked larger (-6 to -9 dB) than R -> L (-3 to -6 dB), but these
  levels come from onset-window peaks on noise (a few dB coarse), so the asymmetry is not established. With the same
  note into both delays (no dead cable) ping-pong on and off look the same, as both delays echo every pass.
  Takes `pp-off`/`pp-on` (without suffix) are invalid: TIME was about 50 ms.
- **TIME jumps and wobble under heavy feedback (take `artifacts-1`, stereo WAV for listening; PING PONG on, dead cable in
  INPUT 2, FEEDBACK just below 2, square 130.9 Hz, MIX fully cw).** The repeats keep bouncing L/R at about -15 dB. Each
  CC12 jump re-pitches what is in the loop at once, by about the delay ratio: 64 -> 32 gives 130.9 -> 214.5 Hz (x1.64;
  delay ratio 1.68), 32 -> 100 gives 214.5 -> 90.7 Hz (x0.42; 0.41), 100 -> 16 gives 90.7 -> 317 Hz (x3.50; 3.73). A
  2 Hz wobble between CC 40 and 80 for 3 s smears the pitch (42-76 Hz per 0.1 s frame), and after TIME returns to CC 64
  the repeats settle at 54 Hz, not 130.8: the wobble leaves a net shift down, and the level drops about 15 dB. Read
  from a 0.1 s pitch track (strongest peak 40-1000 Hz); the WAV is the better evidence.
- **Fast wobble (take `artifacts-2-fastwobble`; FEEDBACK lowered to 12 by the operator after runaway feedback; otherwise as
  `artifacts-1`).** A 6 Hz and a 12 Hz wobble between CC 40 and 80, 3 s each, smear the repeats' pitch widely (about
  90-190 Hz around 130.9; a 0.1 s pitch track cannot follow a 6-12 Hz wobble, so read the WAV). After TIME returns to
  CC 64 the repeats are back at 131-133 Hz, unlike the 2 Hz wobble at FEEDBACK just below 2, which left them at 54 Hz.
  Feedback and wobble rate both differ between the two takes, so which one causes the net shift is not known.
- **2 Hz wobble at FEEDBACK 12 (take `artifacts-3-2hz-fb12`):** inconclusive. The repeats have decayed to the floor
  (about -88 dB, where a steady 131 Hz line sits even before the first note, probably oscillator bleed) before the
  wobble ends, so nothing is left to show a net shift.
- **1 s of 2 Hz wobble at FEEDBACK 1 o'clock (take `artifacts-4-2hz1s-fb1`):** the repeats ring on for about 6 s after
  TIME returns to CC 64, at 133-136 Hz against 130.9 Hz before: a net shift of about +2 to +4 %, not the drop to 54 Hz
  seen in `artifacts-1` (3 s wobble, FEEDBACK just below 2).
- **3 s of 2 Hz wobble at FEEDBACK 1 o'clock (take `artifacts-5-2hz3s-fb1`):** after TIME returns to CC 64 the repeats
  ring on at 130.5-131.5 Hz, within about 0.5 % of the note. So wobble length does not cause the drop to 54 Hz; it
  goes with FEEDBACK just below 2, near self-oscillation. Whether that is a pitch shift of the repeats or a low tone
  the loop builds up near runaway is not known from a peak-frequency track; listen to `artifacts-1.wav` (13-17 s).
- **Ping-pong with a wobble (take `pp-wobble-fb1`, `delay.py seq --preset wobble --pingpong on`; FEEDBACK 1 o'clock,
  2 Hz CC 40-80 for 3 s, square 130.9 Hz, MIX fully cw):** the echoes keep alternating L/R through the wobble (in every
  0.2 s frame one side is loud and the other near the floor). During the wobble each side carries a different pitch
  at the same moment (for example 117.8 Hz on L and 179 Hz on R), since each echo spent its pass at a different delay
  time. After the wobble both sides settle at 130.6-133.2 Hz, so no net shift beyond about 1 %, and the echoes ring
  on for about 8 s.
- **Open:** the first note after a TIME change is often off (by up to 30 ms), which may be the delay time slewing to its
  new value. Wet level falls at short delays (about -2 dB at 110 ms, -5 dB at 50 ms, -7 dB at 42 ms on the square-wave
  turn); cause not known. A noise sweep step at CC 12192 is inconsistent (315-332 ms) and step 2032 read the knob at max,
  turned during the sweep.

## Results 2026-11-19 (Rig A, takes kept locally, not in the repo)
Setup for all takes: MIX noon, FEEDBACK about 10 o'clock, PING PONG off (except `delay-pp-t1/t2`, see below), TIME and SPACING knobs parked at noon (CC
overrides them), one oscillator, CUTOFF fully cw, RES 0, short plucky amp envelope. Ch 13/14 = MAIN OUT L/R. Ch 12 =
VCF 2 OUT (dry, pre-VCA) for `delay-grid-t1`, `delay-spacing-t1`, `delay-sweep-t1/t2`, `delay-quiet-t1`; ch 12 =
DELAY OUT L (100 % wet, the operator patched it) for `delay-tau-t1`, `delay-probe-t1`, `delay-lsb-t1/t2/t3`, `delay-pp-t1/t2` (the last two with ping-pong set up, see below). That patch was
checked in each take: ch 12 is 61-66 dB below its wet peak at the dry onset. The scripts are not yet in `tools/matriarch/`.

- **TIME (`delay-grid-t1`, envelope autocorrelation of 30 ms clicks):** Delay 2 (R) 45.5 / 224.5 / 406.0 ms at CC12
  0 / 64 / 127. The 2026-11-17 noise takes gave 41.65-404.18 ms; the click method reads a few ms longer at the short
  end. Onset-based times (`delay-tau-t1`, below) read another 3-7 ms longer (49 ms at CC12 0). Compare like with like.
- **SPACING at TIME max (`delay-spacing-t1`, CC12 127, CC13 0-127 in steps of 8):** Delay 1 (L) in ms:

  | CC13 | 0 | 8 | 16 | 24 | 32 | 40 | 48 | 56 | 64 | 72 | 80 | 88 | 96 | 104 | 112 | 120 | 127 |
  |---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
  | L | 50.9 | 110.8 | 164.3 | 215.5 | 261.6 | 303.3 | 341.2 | 375.9 | 407.6 | 439.7 | 474.8 | 513.1 | 555.4 | 605.7 | 657.7 | 712.0 | 768.7 |

  Delay 2 stays at 406.0 ms at every CC13 value: measured now, no longer assumed. L - R runs -355 to +363 ms,
  +1.6 ms at CC13 64. The curve is S-shaped: about 7.5 ms per CC step at both ends, about 4 ms near the centre. The
  low end reaches the clamp (50.9 ms), unlike at TIME 203 ms (-157.9 ms, 2026-11-17). TIME max + SPACING max is
  768.7 ms, measured (the 2026-11-17 estimate was about 764).
- **CC steps are a staircase (`delay-tau-t1`, clicks 0.5 s apart).** With TIME stepped one CC value per second (32 to
  40; CC 40 came only at the end of the move, so 8 plateaus for CC 32-39), L sits flat at each value and moves
  2.6-3.2 ms per step, mean 2.86 (140.2, 143.4, 146.3, 148.9, 152.0, 154.7, 157.4,
  160.2 ms). SPACING one step per second (64 to 72 at TIME 64) moves L 3.7-4.1 ms per step (232.3 to 261.7 ms). A
  fast TIME sweep (0 to 127 in 8 s) is linear: L = 49 + 2.89 ms per CC (onset-based).
- **A TIME jump is near-instant (`delay-probe-t1`, one click at a set delay after the CC).** With the CC sent with the
  click, the click already flies at almost the new time: 226.5 ms for TIME 0 -> 64 (new value 229.2), 56.9 ms for
  64 -> 0 (new value 49). A BBD whose clock switches s ms into the flight gives these at s = about 1 ms (up) and about
  10 ms (down): inference from the BBD model. After the jump the time creeps 0.7-2.3 ms over about 0.4 s (64 -> 0
  undershoots to 46.8 ms and returns to 49.1). This is the "settles within 0.45 s" of 2026-11-17; the creep is too
  small to explain the open "first note off by up to 30 ms".
- **Re-pitching while TIME and SPACING move (`delay-sweep-t2`, note 96 at about 1053 Hz, 50 ms every 0.1 s, each
  move 8 s; pitch of the wet lines against the dry note, 0.25 s frames sampled every 0.5 s):**

  | Move | L (cents) | R (cents) |
  |---|---|---|
  | TIME 0 -> 127 | -56 to -83 | -56 to -83 |
  | TIME 127 -> 0 | +62 to +91 | +45 to +83 |
  | SPACING 0 -> 127 at TIME 64 | -89 to -205 | none |
  | SPACING 127 -> 0 at TIME 64 | +92 to +154 | none |
  | TIME up + SPACING down | +24 to +127, changing through the move | -64 to -82 |
  | TIME down + SPACING up | -32 to -87 | +45 to +101 |

  The TIME sweep predicts 1200 log2(1 - dτ/dt) = -80 cents from its 45 ms/s slope, which the measurement matches.
  Opposite moves detune L against R by up to about 200 cents, and L's shift changes during the move because the SPACING curve is S-shaped.
  At 16 CC steps per second the per-step jumps merge into a steady shift; at a slow rate each step re-pitches the
  loop for one delay length (by the BBD model, about -35 cents for one 2.86 ms step (the mean) at 140 ms: inference). The
  +-18 cent lines in every frame are the 10 Hz retrigger comb, not the delay.
- **No clock tones (`delay-quiet-t1`, no notes, six TIME/SPACING settings from 45 to 769 ms, 3 s each):** no tone
  between 200 Hz and 23 kHz changes with TIME or SPACING, down to about -120 dBFS per 0.5 Hz bin. The 8, 8.75 and
  16 kHz lines are the same at every setting (interface lines, see the skill), and 1054 Hz with its odd harmonics is
  the held oscillator leaking past the VCA. Section 2 expects f_clk = 10.2 kHz at 400 ms and 5.25 kHz at 780 ms for
  N = 8192; no line was seen there. Either N differs or the filters bury the line below -120 dBFS; this take cannot
  tell which (the clock stays open).
- **Ping-pong while TIME and SPACING move (`delay-pp-t2`; PING PONG on, dead cable in delay INPUT 2, FEEDBACK
  about 1 o'clock, MIX noon, ch 12 = DELAY OUT L; note 96 clicks every 0.4 s through each 4 s move).**
  - Static, TIME 64 (three single clicks): echoes alternate strictly L, R, L, R, 229.4-229.6 ms apart, with the other
    side at the floor (50 dB or more down). Same-side loss per round trip is 8-9 dB at high level, steeper at the tail
    (10-15 dB once the echo is below about -50 dBFS). Peaks: L -26, R -32, L -35, R -39, L -43, R -48, L -52, R -58,
    L -66 dBFS (L from DELAY OUT L, R from MAIN R: different paths, so compare same-side steps only).
  - Each pass re-pitches by the slope, and passes add up. L carries odd multiples, R even ones:

    | Move (4 s) | per pass (predicted) | L (cents) | R (cents) |
    |---|---|---|---|
    | TIME 64 -> 127 | -78 | -73 to -91, -199 to -250, -367 to -426 | -130 to -180, -296 to -350, -450 to -477 |
    | TIME 127 -> 32 | +114 | +107 to +138, +333 to +371, +571 to +597 | +223 to +273, +389 to +488, +686 to +721 |

    The per-pass slope changes along the move (TIME is linear in CC, the ratio is not), so the multiples are rough.
  - The pitches stay after the move stops: after TIME 127 -> 32, L sits at +330 to +342 cents and R at +594 to +620
    cents (5.2-5.4 x the predicted per-pass shift, so not a clean even multiple) until the echoes die (about 0.8 s), about 260 cents apart. After TIME 100 -> 32 with SPACING 20 -> 110, L
    alternates about -83 / -194 cents and R holds -107 to -136 cents for 2 s or more.
  - The opposite TIME/SPACING moves give mixed lines that this readout does not model (`delay-pp-t2.wav` for listening).
  - R is read from MAIN R with +-25 cents around the note cut out (the dry clicks), so unshifted wet R is not seen.
  - `delay-pp-t1` (one click before each move) is valid for the static pattern only: the echoes reached the
    -99 dBFS oscillator bleed within about 2 s, before most of each move.
- **CC44 / CC45 (LSB) work, but CC12 is dropped when CC44 follows back-to-back (`delay-probe-t1`,
  `delay-lsb-t1/t2/t3`).**
  - CC44 sent alone, LSB 0 -> 127 -> 0 (`delay-lsb-t3`): L moves 2.8 / 2.9 / 3.1 ms at CC12 32 / 64 / 100
    (134.2-137.0, 225.8-228.7, 328.9-332.0 ms), about 0.7 ms per 32 LSB counts, monotonic, up and down within 0.2 ms.
    So the LSB spans one MSB step and TIME is 14-bit over MIDI. At CC12 0 the range is smaller (about 1.4 ms in
    `delay-lsb-t2`, near the TIME minimum; 2.7 ms in `delay-probe-t1`).
  - CC12 sent alone reads like an LSB between about 32 and 64 (135.0-135.1 ms at CC12 32), not like LSB 0.
  - CC45 at CC13 64: L 134.6 -> 138.4 ms over LSB 0-127, about one MSB step.
  - CC12 then CC44 with no gap: the MSB is not applied (L stays at the old MSB, LSB applied). Seen in four takes, and
    it is why the 2026-11-17 sweep "stuck at 222.8/225.6 ms". With 2, 5, 10, 20, 50 or 100 ms between them the MSB
    applies every time (`delay-lsb-t3`, one click each). CC44 before CC12: the MSB applies and the LSB shows no
    effect.

## Results go to
- `bbd-research.md` 8.1: each "inference" row becomes "measured", with date and settings.
- The Matriarch profile struct (d21):
  - stages and clock range, with any range switching;
  - filter fc and order per Bright/Dark;
  - hold time T_h;
  - compander tau;
  - limiting threshold and ratio;
  - THD against level;
  - noise per f_clk.
