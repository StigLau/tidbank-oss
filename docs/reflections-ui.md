# Reflections: the UI of the plugin family

Notes to pick up after the running plugin has been tried. Nothing here is designed or decided. Routing and the split into plugins are discussed in `docs/reflections-routing.md`.

## The maintainer, 2026-11-16 (thoughts, lightly edited)

- **What a face looks like.** If we end up with several faces (or the equivalent), what do they each look like?
- **Reels, the looper:**
  - the different input sources;
  - the reel itself;
  - starting and stopping;
  - a "jog wheel": an endless encoder that speeds the reel up, slows it down, halts it, and even runs the tape in reverse;
  - an indicator for how fast the reel is running.
- **BBD:** does it look like the Matriarch's delay, or something else?
- **Tape delay:** what are the overarching thoughts on its UI, and what inspires it?
- Not to be started yet: the running application had not been looked at.

## What is already on record (for the discussion, not a proposal)

- **A review model's surface proposal.** One small editor panel per plugin, plus a shipped Live rack with 8 macros (Rec, Print, Undo, Reel level, Intensity, Division, Echo, Mode) that acts as the shared face.
  - Reels: 2 knob pages.
  - TidBank: its 3 pages, unchanged.
  - The tape-ageing plugin: its own.
  - Ableton Move is no longer a requirement, so the knob pages matter less than the editors.
- **The jog wheel conflicts with the looper plan as written.** `docs/plan-looper.md` section 2.2 locks the reel to the host bar grid, with "no inertia and no nudge on the reel", because the sketch ruled out drifting against the DAW. Speeding up, halting or reversing with a jog wheel takes the reel off the grid.
  - Possible readings to discuss:
    - a performance mode that leaves the grid and snaps back on release;
    - scrubbing only while the host is stopped;
    - a change to the plan.
  - Reverse playback is not in the plan at all.
  - The tape engine already turns any speed curve v(t) into the right pitch on every head (whitepaper section 2), so the maths exists. The question is the reel's relation to the DAW grid.
  - **Position of the maintainer, 2026-11-16, on the jog wheel:**
    - nudging is a performance mode;
    - running in reverse, and staying there, is only a feature for going completely off the rails, when the DAW is paused;
    - nudging is a later consideration, not for now or the near future; it is a loose idea.
    - So the plan's grid lock (section 2.2) stands for v1. (Nudge was later added to Reels in a limited form; see the status block of `plan-looper.md`.)
- **BBD.** The BBD part is steered by the Matriarch concept (d16): two lines, SPACING, ping-pong, Dark/Bright (`docs/research/bbd-matriarch.md`). Whether that means its look or only its behaviour is open.
- **Tape delay.** The engine's references are the RE-201 (heads, modes 1-12) and the Volante (spacing, ping-pong as an X of writer heads, d11).
- **Existing look in the family.** A separate tape-ageing plugin has a Blender-rendered faceplate styled on Tandberg cassette decks: VU meters, reels that spin in sync with tape speed, filmstrip knobs. That is a precedent for "reels that show the speed".
- **Today's TidBank editor** is plain controls (`plugin/PluginEditor`). `make ui` renders a PNG of it without a host.

## Questions to settle in that discussion

1. One visual language across the family, or a look per device (Matriarch-like BBD, Space Echo-like tape, Tandberg-like ageing plugin)?
2. Reels' input sources: live input only, or also other tracks or plugins? This ties to Print = bounce of the chain end.
3. Jog wheel and reverse: which of the readings above?
4. Speed indicator: spinning reels (as in the ageing plugin), a numeric speed ratio, or both?
5. Who renders the UI: hand-drawn JUCE, Blender filmstrips, or something else? This depends on how much of the ageing plugin's UI work can be shared.
