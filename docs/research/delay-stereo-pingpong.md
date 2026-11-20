# Stereo, spacing and ping-pong in the reference delays (and ideas from the MIDI MuRF)

Collected 2026-11-16 by a Sonnet research agent. P = primary source (manufacturer manual or page), S = secondary (forum, retailer, transcription). Anything marked "not found" is still open: measure it on a real Matriarch rather than guess. Earlier Matriarch notes: `bbd-matriarch.md`.

## Moog Matriarch stereo delay (P: manual, https://cdn.inmusicbrands.com/Moog/Matriarch/Matriarch_Manual_012023.pdf, pp. 32-35, 56, 62-63)
- Two BBD delays sharing parameters. One TIME knob sets both: 35 ms to 780 ms.
- **SPACING:** "creates an offset in the delay time of Delay 1 relative to Delay 2". It is bipolar, centre = same. CW lengthens Delay 1 and CCW shortens it. "If TIME is already at its highest level, raising SPACING may have little effect."
  - Synced: SPACING picks "divisions or multiplications of the clock rate" for Delay 1.
  - Not found: the free range (ms or ratio) and the synced division list.
- **PING PONG:** "the output of Delay 1 is fed to the input of Delay 2, and the output of Delay 2 is fed back to the input of Delay 1".
  - Delay 1 = left, Delay 2 = right. One FEEDBACK for both; there are separate CV inputs for Delay 1 and Delay 2 feedback.
  - Not found: where the input enters in ping-pong. A forum tip is to patch a dummy cable into INPUT 1 or 2 so only one line gets the signal (S, https://forum.moogmusic.com/t/matriarch-delay-time/20614).
- **Normal stereo:** VCA 1 → Delay 1 → left, VCA 2 → Delay 2 → right. MIX crossfades dry and wet.
- **Global settings:** Dark/Bright filter. Self-oscillation above about 2 o'clock feedback.

## Boss DM-2 / DM-2W (P: https://www.boss.info/products/dm-2w/)
- Mono input; no ping-pong, no tap.
- The DM-2W has two outputs that split wet and dry (OUTPUT = delay, DIRECT OUT = dry when both are used). That is not stereo.
- MODE switch: Standard 20-300 ms (original DM-2) or Custom up to 800 ms. The RATE jack takes an expression pedal.

## MXR Carbon Copy family (P: jimdunlop.com product pages; the manuals were not readable)
- M169, Bright and Mini: Delay, Mix, Regen, optional modulation (internal width/rate trims), up to 600 ms. No stereo or ping-pong is mentioned for any of them.
- Deluxe M292:
  - delay up to 1.2 s;
  - tap tempo with divisions (dotted 8th, 8th, triplet, 16th);
  - a warm/bright tone switch;
  - modulation speed and width;
  - presets, a dry-kill switch and an expression jack.
  - Again, no stereo is mentioned.

## Strymon Volante (P: https://www.strymon.net/manuals/Volante_UserManual_RevE.pdf)
- Stereo in and out.
- No ping-pong mode. Each head is panned instead (hold its feedback button and turn TIME); feedback does not cross channels.
  - "When all heads are panned to center, a psychoacoustic stereo process is applied."
- SPACING has four settings: even, triplet, golden, silver. TIME sets head 4, and heads 1-3 subdivide it.

## Moog MF-104M / MF-104Z / Minifooger MF Delay (S: transcriptions and retailers; Moog's PDFs were blocked)
- MF-104M:
  - mono; 40-400 ms (Short) or 80-800 ms (Long);
  - a FEEDBACK INSERT send/return inside the loop;
  - a six-waveform LFO on delay time (0.05-50 Hz, syncs to tap or MIDI clock);
  - MIDI, tap and Spillover;
  - no ping-pong or spacing.
- MF-104Z: 50-1000 ms, feedback insert, no ping-pong.
- Minifooger MF Delay: time, feedback, mix and drive; four BBDs, about 700 ms; the expression input controls feedback or time; mono.

## Other stereo delays
- **Line 6 DL4 ping-pong:** two lines, each feeding the other. TWEAK sets the right line as a percentage of the left (S).
- **Boss DD-8:** independent L/R by default, plus Pan (ping-pong) and Wide modes (P, routing details not read).
- **Strymon Brig:** "dual cross-coupled ping-pong delay lines" with golden-ratio timing (P).

## Moog MF-105M MIDI MuRF: ideas for echoes (P: manual, retailer-hosted copy https://r2.gear4music.com/media/8/86064/download_86064_1.pdf)
- **What it is:**
  - 8 resonant band-pass filters (BASS or MIDS voicing), each with a level slider;
  - an "Animation" sequencer: 8 channels of up to 64 steps each, of different lengths, each step triggering that band's envelope;
  - 24 patterns;
  - one ENVELOPE knob that morphs all envelopes from choppy to swirling;
  - RATE with tap or MIDI clock divisions (whole to 1/64, dotted and triplet);
  - a sine LFO that moves all band centres together.
- **Stereo:** with both outputs used, odd bands go left and even bands go right (p. 14).
- **Ideas that could carry over to TidBank** (not decided):
  1. Rhythmic, tempo-synced filtering of the echoes or the feedback through a small band array.
  2. Odd/even band split to L/R, so the stereo comes from filtering instead of timing. It pairs with Spacing and ping-pong.
  3. Polymetric step lengths, so echoes change over the repeats.
  4. One morph knob for a complex modulation.
  5. A delay-time LFO, as on the MF-104M.

## What TidBank does with this
- Ping-pong: mono sum into the left line only; left → right → left; one Intensity.
- Spacing (left against right):
  - free: × 2^(spacing/100), so half to double;
  - synced: ±6 steps through the Division list.
- These are our choices where the Matriarch manual is silent. To check on a real Matriarch: the real Spacing range, the synced divisions, and where the input goes in ping-pong.
