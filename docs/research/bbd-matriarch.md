# Moog Matriarch stereo delay: reference for the BBD engine

Collected 2026-11-15 by a Sonnet research agent from the official manual (Jan 2023, https://cdn.inmusicbrands.com/Moog/Matriarch/Matriarch_Manual_012023.pdf, pp. ~32-35, 56, 62-63) and secondary sources. The unknowns below are measured on the bench, not guessed. The delay was measured on 2026-11-17: see "Measured" below and `bbd-research.md` 8.1b.

## From the manual (primary)
- Two BBD-based analog delays sharing some parameters, based on the Moog 500 Series Analog Delay.
- Time: 35 ms (CCW) to 780 ms (CW) per the manual; measured 41.65-404.18 ms (below). Longer via tap tempo / patching gives "noisy or lo-fi" trails (BBD bandwidth falls with delay: matches D3 `f_clk = N/(2 tau)`).
- Knobs: TIME (both lines, rate LED), SPACING (bipolar; centre = equal; CW lengthens Delay 1 relative to Delay 2, Delay 2 is the reference), FEEDBACK, MIX (dry/wet crossfade).
- Buttons: SYNC/TAP (internal arp/seq clock or rear SYNC IN; 3+ taps), PING PONG. LED: red free, green internal, yellow external clock.
- Synced: TIME picks clock divisions/multiples, SPACING does the same for Delay 1.
- Ping-pong: Delay 1 output feeds Delay 2 input, Delay 2 output feeds back to Delay 1 (only audible with feedback).
- Self-oscillation above ~2 o'clock feedback.
- Patch: INPUT 1/2, FB CV, MIX CV, TIME 1/2 CV (+-5 V, summed with knobs); rear DELAY OUT L/R (100 % wet), FB 2 CV.
- Global settings: Delay Filter Brightness Dark/Bright (default Bright); Delay Sync CV Bend (TIME CV bends synced time up to +-33 %, "choral motion or warble").
- MIDI: Time, Spacing, Ping Pong, Sync.

## Measured 2026-11-17 (`matriarch-measurement.md`, Results 2026-11-17)
- TIME 41.65-404.18 ms, linear in CC12 MSB (42.04 + 2.847 ms per step). Delay 1 can go further with SPACING: TIME max + SPACING max is about 764 ms (inferred; measured 2026-11-19: 768.7 ms).
- SPACING adds milliseconds to Delay 1 (not a ratio): -157.9 to +360.2 ms at TIME 203 ms, clamped at 45.0 ms; at TIME min the same curve, 4.4 ms lower (Delay 1 cannot go below the minimum). Delay 2 is assumed to stay put (p. 32), not checked (checked 2026-11-19: it does).
- Delay filter: Bright -3 dB at about 2.3 kHz (2.09 kHz at 404 ms), Dark 1.41 kHz (at 203 ms only), about -50 dB/oct; Bright barely moves with TIME over 43-404 ms. Each repeat passes it again, so feedback goes through the filter.
- Feedback loss per pass from -14.5 dB (9 o'clock) to -0.65 dB (just below 2); self-oscillates past that. The path does not invert.
- Turning TIME re-pitches what is in the loop by t_old/t_new and the repeats keep that pitch; TIME settles within 0.45 s.
- Ping-pong alternates the echoes L/R.

## Measured 2026-11-19 (`matriarch-measurement.md`, Results 2026-11-19)
- SPACING at TIME max: Delay 1 50.9-768.7 ms (-355 to +363 ms against Delay 2, S-shaped: about 7.5 ms per CC step at
  the ends, 4 ms near the centre); Delay 2 stays at 406.0 ms. 768.7 ms is the longest Delay 1 time over MIDI.
- Delay time moves in steps, one per CC value (2.6-3.2 ms per TIME step, mean 2.86; 3.7-4.1 ms per SPACING step near
  the centre). A jump takes effect within about 1-10 ms (inferred from flight times with a BBD model), then creeps 0.7-2.3 ms over about 0.4 s.
- Sweeping re-pitches the repeats by the slope: TIME 0 -> 127 in 8 s gives about -80 cents on both lines; opposite
  TIME and SPACING moves detune L against R by up to about 200 cents.
- Ping-pong (dead cable in INPUT 2): echoes alternate strictly L/R; a TIME move re-pitches each pass and the passes add
  up, L holding roughly odd and R roughly even multiples, so the two sides end up detuned (about 260 cents apart after TIME 127 -> 32
  in 4 s) and keep that pitch until the echoes die.
- No clock tone between 200 Hz and 23 kHz above about -120 dBFS at any TIME/SPACING (N or filtering, not known).
- CC44/45 are the LSBs of TIME/SPACING and span one MSB step (TIME is 14-bit over MIDI, about 0.7 ms per 32 counts),
  but CC12 followed back-to-back by CC44 is dropped; 2 ms between them is enough.

## Still not measured or found
BBD chip type and count (500 Series retailer listing says four NOS Panasonic chips, secondary), clock rate, compander and limiting, the 54 Hz drop after a TIME wobble near runaway.

## Secondary (forums, unverified)
Noise "like an old dial-up modem", much lower in Dark mode; some find it muffled (expected BBD bandwidth); warble/ringing in Sync/Tap probably fixed in firmware 1.3.0.

## What it suggests for TidBank
- Matriarch SPACING (bipolar L/R offset with one line as reference) is the stereo analogue of our head `spacing`; a BBD UI could reuse `head_target` semantics. Measured, it adds milliseconds; the plugin's BBD Spacing is a ratio (Time x 2^(spacing/100)), so they differ. the author kept the ratio (d34, 2026-11-18).
- Ping-pong = cross-feed of the two lines: the same X pattern as the Volante plan (D8).
- Dark/Bright = the BBD reconstruction filter corner; maps to our loop LPF plus fixed BBD filters.
- Sync CV Bend (+-33 %) = nudge with a wider range: our cents nudge already covers it.
- The Matriarch's TIME/SPACING move in steps, one per CC value (128 MSB steps; 14-bit with the LSB), and by the BBD
  model each step re-pitches the loop (inference); a smooth (per-sample) Time in
  the plugin is the cleaner behaviour, and stepping would be a deliberate "Matriarch character" option, not a must.
