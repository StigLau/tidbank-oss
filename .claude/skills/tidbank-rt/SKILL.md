---
name: tidbank-rt
description: Real-time audio rules for TidBank (dsp/ and the JUCE plugin/): no allocation, locks or I/O on the audio thread, smoothing, denormals, latency. Use before changing anything that runs per sample or per block.
---

# Audio thread rules

Full checklist: `docs/rt-audio-checklist.md` (condensed from a cited research run).

- No allocation, locks, file/network I/O or logging in `process`/`processBlock`. Allocate in `prepare()` (`DelayLine::prepare`, `TapeTransport` rings, sinc table).
- State in double; flush filter states below 1e-15 (subnormals: `health` reports them).
- Parameters: the wrapper copies APVTS atomics into a `TapeParams` per block. Time-type params go through the transport inertia; the rest through 20 ms smoothers. The first `setParams` after `prepare`/`reset` snaps.
- Latency: the plugin reports 0. Never add latency to the dry path (no oversampling, no linear-phase filters there). Wet-path look-ahead is compensated in the head read offset (`kReadOffset` = 1.5: ADAA half sample + read-before-push); the sinc reach (16 samples back) must stay far below the shortest head delay (40 ms = 1920 samples at 48 kHz).
- `reset()` may run on the audio thread (JUCE): it clears in place (`DelayLine::clear`), never re-`prepare`s.
- Anything that jumps (mode, spacing, head distance) cross-fades or glides; ears `s4_click_*` assert it.

- Debug probe (`TIDBANK_DEBUG_PROBE`): compiled out by default; hooks in the engines are `if (tracer_)` reads of state, never writes; the audio thread only pushes fixed-size records into a preallocated SPSC ring (full = drop + count). All I/O and config on the message thread (Timer). Any new hook must keep ears `p_null_*` byte-identical.

## Before claiming a DSP change works
`make test && make ears`, report the numbers. "Sounds right" is the human listener's call (S8-style listening set), never the agent's.
